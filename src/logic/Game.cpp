#include "Game.h"
#include <algorithm>
#include <cmath>

static float dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector3 cross(const Vector3& a, const Vector3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

static Vector3 normalized(const Vector3& value) {
    float length = std::sqrt(dot(value, value));
    if (length < 0.0001f) return {0, 0, 0};
    return {value.x / length, value.y / length, value.z / length};
}

static Vector3 scaled(const Vector3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

static Vector3 added(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector3 rotateAround(const Vector3& value, const Vector3& axis, float angle) {
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return added(added(scaled(value, cosine), scaled(cross(axis, value), sine)),
                 scaled(axis, dot(axis, value) * (1.0f - cosine)));
}

Game::Game(int mazeSize, float seconds)
    : mazeData(mazeSize), px(0), py(0.5f), pz(0), lookDirection{1, 0, 0},
            gravityUp{0, 1, 0}, rollFrom{0, 1, 0}, rollAxis{1, 0, 0},
            rollAngle(0), rollTime(0.25f), transitionFromPosition{0, 0.5f, 0},
            transitionFromDirection{1, 0, 0}, attachedToSurface(false),
      hasWon(false), hasLost(false), timeLeftSec(seconds), elapsedSec(0), points(0) {
    int n = mazeData.size();
    seen.assign(n * n, 0);
    lookDirection = (mazeData.cell(0, 1) != '#')
        ? Vector3{1, 0, 0} : Vector3{0, 0, 1};              // face the open direction at the start
    reveal();
}

bool Game::isWall(float x, float z) const {
    int c = (int)std::floor(x + 0.5f);
    int r = (int)std::floor(z + 0.5f);
    return mazeData.cell(r, c) == '#';
}

bool Game::canMove(float x, float z) const {
    const float m = 0.1f;
    return !isWall(x - m, z - m) && !isWall(x + m, z - m) &&
           !isWall(x - m, z + m) && !isWall(x + m, z + m);
}

bool Game::hasSurfaceSupport(float x, float z) const {
    int r = (int)std::floor(z + 0.5f);
    int c = (int)std::floor(x + 0.5f);
    if (std::fabs(gravityUp.y) > 0.5f) return mazeData.cell(r, c) != '#';
    if (gravityUp.x > 0.5f) return mazeData.cell(r, c - 1) == '#';
    if (gravityUp.x < -0.5f) return mazeData.cell(r, c + 1) == '#';
    if (gravityUp.z > 0.5f) return mazeData.cell(r - 1, c) == '#';
    if (gravityUp.z < -0.5f) return mazeData.cell(r + 1, c) == '#';
    return false;
}

void Game::beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition,
                                  bool attached) {
    transitionFromPosition = {px, py, pz};
    transitionFromDirection = lookDirection;
    rollFrom = cameraUp();
    rollTime = 0.0f;
    rollAngle = std::acos(std::clamp(dot(rollFrom, newUp), -1.0f, 1.0f));
    rollAxis = normalized(cross(rollFrom, newUp));
    if (dot(rollAxis, rollAxis) < 0.5f) {
        rollAxis = normalized(cross(lookDirection, rollFrom));
        if (dot(rollAxis, rollAxis) < 0.5f) rollAxis = {1, 0, 0};
    }
    lookDirection = normalized(rotateAround(lookDirection, rollAxis, rollAngle));
    gravityUp = newUp;
    px = targetPosition.x;
    py = targetPosition.y;
    pz = targetPosition.z;
    attachedToSurface = attached;
}

bool Game::findAimedSurface(Vector3& normal, Vector3& contact) const {
    const float maxDistance = 2.5f;
    float nearest = maxDistance;
    bool found = false;

    auto consider = [&](float distance, const Vector3& surfaceNormal, const Vector3& point) {
        if (distance > 0.02f && distance < nearest) {
            nearest = distance;
            normal = surfaceNormal;
            contact = point;
            found = true;
        }
    };

    if (lookDirection.y > 0.001f) {
        float distance = (1.0f - py) / lookDirection.y;
        float hitX = px + lookDirection.x * distance;
        float hitZ = pz + lookDirection.z * distance;
        if (distance > 0.0f && distance < nearest && mazeData.cell(
                (int)std::floor(hitZ + 0.5f), (int)std::floor(hitX + 0.5f)) != '#')
            consider(distance, {0, -1, 0}, {hitX, 1.0f, hitZ});
    }

    int reach = (int)std::ceil(maxDistance) + 1;
    int r0 = std::max(0, row() - reach);
    int r1 = std::min(mazeData.size() - 1, row() + reach);
    int c0 = std::max(0, col() - reach);
    int c1 = std::min(mazeData.size() - 1, col() + reach);

    auto testWallFace = [&](int r, int c, int wallR, int wallC,
                            const Vector3& faceNormal, float plane, bool verticalX) {
        if (mazeData.cell(r, c) == '#' || mazeData.cell(wallR, wallC) != '#') return;
        if (dot(lookDirection, faceNormal) >= -0.001f) return;

        float component = verticalX ? lookDirection.x : lookDirection.z;
        if (std::fabs(component) < 0.001f) return;
        float origin = verticalX ? px : pz;
        float distance = (plane - origin) / component;
        if (distance <= 0.02f || distance >= nearest) return;

        float hitX = px + lookDirection.x * distance;
        float hitY = py + lookDirection.y * distance;
        float hitZ = pz + lookDirection.z * distance;
        if (hitY < 0.0f || hitY > 1.0f ||
            hitX < c - 0.5f || hitX > c + 0.5f ||
            hitZ < r - 0.5f || hitZ > r + 0.5f) return;
        consider(distance, faceNormal, {hitX, hitY, hitZ});
    };

    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            float x = (float)c;
            float z = (float)r;
            testWallFace(r, c, r - 1, c, {0, 0, 1}, z - 0.5f, false);
            testWallFace(r, c, r + 1, c, {0, 0, -1}, z + 0.5f, false);
            testWallFace(r, c, r, c - 1, {1, 0, 0}, x - 0.5f, true);
            testWallFace(r, c, r, c + 1, {-1, 0, 0}, x + 0.5f, true);
        }
    }
    return found;
}

bool Game::attachToSurface(const Vector3& normal, const Vector3& contact) {
    Vector3 targetPosition = added(contact, scaled(normal, 0.22f));
    int targetRow = (int)std::floor(targetPosition.z + 0.5f);
    int targetCol = (int)std::floor(targetPosition.x + 0.5f);
    if (!canMove(targetPosition.x, targetPosition.z)) return false;

    bool supported = false;
    if (normal.y < -0.5f) supported = mazeData.cell(targetRow, targetCol) != '#';
    else if (normal.x > 0.5f) supported = mazeData.cell(targetRow, targetCol - 1) == '#';
    else if (normal.x < -0.5f) supported = mazeData.cell(targetRow, targetCol + 1) == '#';
    else if (normal.z > 0.5f) supported = mazeData.cell(targetRow - 1, targetCol) == '#';
    else if (normal.z < -0.5f) supported = mazeData.cell(targetRow + 1, targetCol) == '#';
    if (!supported) return false;

    targetPosition.y = normal.y < -0.5f ? 0.78f : std::clamp(targetPosition.y, 0.1f, 0.9f);
    beginSurfaceTransition(normal, targetPosition, true);
    return true;
}

bool Game::transitionToAdjacentWall(int side) {
    int r = row(), c = col();
    Vector3 normal;
    Vector3 contact{px, py, pz};
    int wallR = r, wallC = c;
    if (side == 0) {
        wallR--;
        normal = {0, 0, 1};
        contact.z = r - 0.5f;
    } else if (side == 1) {
        wallR++;
        normal = {0, 0, -1};
        contact.z = r + 0.5f;
    } else if (side == 2) {
        wallC--;
        normal = {1, 0, 0};
        contact.x = c - 0.5f;
    } else {
        wallC++;
        normal = {-1, 0, 0};
        contact.x = c + 0.5f;
    }
    if (mazeData.cell(wallR, wallC) != '#') return false;
    return attachToSurface(normal, contact);
}

bool Game::transitionToNearestWall(float x, float z) {
    int r = (int)std::floor(z + 0.5f);
    int c = (int)std::floor(x + 0.5f);
    if (mazeData.cell(r, c) == '#') return false;

    float bestScore = 1000.0f;
    Vector3 chosenNormal{0, 0, 0};
    Vector3 chosenContact{0, 0, 0};
    bool found = false;
    auto consider = [&](int wallR, int wallC, const Vector3& normal,
                        const Vector3& contact, float distance) {
        if (mazeData.cell(wallR, wallC) != '#') return;
        float score = distance - 0.2f * dot(normal, gravityUp);
        if (score < bestScore) {
            bestScore = score;
            chosenNormal = normal;
            chosenContact = contact;
            found = true;
        }
    };

    float clampedX = std::clamp(x, c - 0.5f, c + 0.5f);
    float clampedZ = std::clamp(z, r - 0.5f, r + 0.5f);
    consider(r - 1, c, {0, 0, 1}, {clampedX, py, r - 0.5f}, std::fabs(z - (r - 0.5f)));
    consider(r + 1, c, {0, 0, -1}, {clampedX, py, r + 0.5f}, std::fabs(z - (r + 0.5f)));
    consider(r, c - 1, {1, 0, 0}, {c - 0.5f, py, clampedZ}, std::fabs(x - (c - 0.5f)));
    consider(r, c + 1, {-1, 0, 0}, {c + 0.5f, py, clampedZ}, std::fabs(x - (c + 0.5f)));
    return found && attachToSurface(chosenNormal, chosenContact);
}

void Game::toggleGravity() {
    if (attachedToSurface) {
        beginSurfaceTransition({0, 1, 0}, {px, 0.5f, pz}, false);
        return;
    }

    Vector3 normal, contact;
    if (findAimedSurface(normal, contact)) attachToSurface(normal, contact);
}

void Game::reveal() {
    int n = mazeData.size();
    int r = row(), c = col();
    if (r < 0 || r >= n || c < 0 || c >= n) return;
    for (int i = r - 1; i <= r + 1; i++)
        for (int j = c - 1; j <= c + 1; j++)
            if (i >= 0 && i < n && j >= 0 && j < n && seen[i * n + j] == 0)
                seen[i * n + j] = 1;
    seen[r * n + c] = 2;
}

void Game::update(const InputState& in) {
    if (hasWon || hasLost) return;                 // game is frozen when finished

    elapsedSec += in.dt;                           // time taken (counts up)

    timeLeftSec -= in.dt;                          // time left (counts down)
    if (timeLeftSec <= 0.0f) {
        timeLeftSec = 0.0f;
        hasLost = true;
        return;
    }

    rollTime = std::min(rollTime + in.dt, 0.25f);
    if (in.gravityToggle && rollTime >= 0.25f) toggleGravity();
    if (rollTime < 0.25f) {
        reveal();
        return;
    }

    lookDirection = normalized(rotateAround(lookDirection, gravityUp, -in.mouseDX * 0.0025f));
    Vector3 cameraRight = normalized(cross(lookDirection, gravityUp));
    float currentPitch = std::asin(std::clamp(dot(lookDirection, gravityUp), -1.0f, 1.0f));
    float targetPitch = std::clamp(currentPitch - in.mouseDY * 0.0025f, -1.5f, 1.5f);
    lookDirection = normalized(rotateAround(lookDirection, cameraRight, targetPitch - currentPitch));
    cameraRight = normalized(cross(lookDirection, gravityUp));

    Vector3 moveForward = normalized(added(lookDirection, scaled(gravityUp, -dot(lookDirection, gravityUp))));
    Vector3 movement = added(scaled(moveForward, in.forward), scaled(cameraRight, in.right));
    float movementLength = std::sqrt(dot(movement, movement));
    if (movementLength > 1.0f) movement = scaled(movement, 1.0f / movementLength);

    float step = 3.0f * in.dt;
    // Keep X before Z and stop immediately when either axis changes surfaces.
    if (moveHorizontal(movement.x, step, true) || moveHorizontal(movement.z, step, false)) {
        reveal();
        return;
    }

    if (attachedToSurface && std::fabs(gravityUp.y) < 0.5f) {
        float nextY = py + movement.y * step;
        if (nextY < 0.1f) {
            beginSurfaceTransition({0, 1, 0}, {px, 0.22f, pz}, false);
            reveal();
            return;
        }
        if (nextY > 0.9f) {
            beginSurfaceTransition({0, -1, 0}, {px, 0.78f, pz}, true);
            reveal();
            return;
        }
        py = nextY;
    } else if (attachedToSurface) {
        py = std::clamp(py + movement.y * step, 0.1f, 0.9f);
    } else {
        py = 0.5f;
    }

    reveal();
    if (mazeData.cell(row(), col()) == 'E') {
        hasWon = true;
        points = mazeData.size() * 100 + (int)std::ceil(timeLeftSec) * 10;
    }
}

bool Game::moveHorizontal(float movement, float step, bool alongX) {
    float nextX = alongX ? px + movement * step : px;
    float nextZ = alongX ? pz : pz + movement * step;
    if (canMove(nextX, nextZ)) {
        if (!attachedToSurface || hasSurfaceSupport(nextX, nextZ)) {
            (alongX ? px : pz) = alongX ? nextX : nextZ;
        } else if (std::fabs(gravityUp.y) < 0.5f) {
            if (!transitionToNearestWall(nextX, nextZ))
                beginSurfaceTransition({0, 1, 0}, {nextX, 0.22f, nextZ}, false);
            return true;
        }
    } else if (std::fabs(movement) > 0.001f) {
        int side = alongX ? (movement > 0.0f ? 3 : 2) : (movement > 0.0f ? 1 : 0);
        return transitionToAdjacentWall(side);
    }
    return false;
}

float Game::transitionBlend() const {
    return std::clamp(rollTime / 0.25f, 0.0f, 1.0f);
}

Vector3 Game::position() const {
    float blend = transitionBlend();
    return added(scaled(transitionFromPosition, 1.0f - blend), scaled({px, py, pz}, blend));
}
float Game::x() const { return position().x; }
float Game::y() const { return position().y; }
float Game::z() const { return position().z; }
float Game::yaw() const { return std::atan2(lookDirection.z, lookDirection.x); }
Vector3 Game::cameraDirection() const {
    float blend = transitionBlend();
    return normalized(added(scaled(transitionFromDirection, 1.0f - blend),
                            scaled(lookDirection, blend)));
}
Vector3 Game::cameraUp() const {
    return normalized(rotateAround(rollFrom, rollAxis, rollAngle * transitionBlend()));
}
bool Game::surfaceAttached() const { return attachedToSurface; }
int Game::row() const { return (int)std::floor(pz + 0.5f); }
int Game::col() const { return (int)std::floor(px + 0.5f); }

int Game::seenState(int r, int c) const {
    int n = mazeData.size();
    if (r < 0 || r >= n || c < 0 || c >= n) return 0;
    return seen[r * n + c];
}

float Game::timeLeft() const { return timeLeftSec; }
float Game::timeTaken() const { return elapsedSec; }
int Game::score() const { return points; }
bool Game::won() const { return hasWon; }
bool Game::lost() const { return hasLost; }
bool Game::finished() const { return hasWon || hasLost; }
const MazeData& Game::maze() const { return mazeData; }
