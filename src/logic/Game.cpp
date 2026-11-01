#include "Game.h"
#include <algorithm>
#include <cmath>

using namespace MazeGeometry;

// Compute the scalar product used to project movement, measure alignment and recover rotation angles.
static float dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Compute a right-handed perpendicular vector for camera axes and surface-transition rotation axes.
static Vector3 cross(const Vector3& a, const Vector3& b) {
    return { a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x };
}

// Return a unit-length direction, or a zero vector when the input is too short to normalize safely.
static Vector3 normalized(const Vector3& value) {
    float length = std::sqrt(dot(value, value));
    if (length < 0.0001f) return { 0, 0, 0 };
    return { value.x / length, value.y / length, value.z / length };
}

// Multiply every vector component by the same scalar without changing its direction for a positive
// factor.
static Vector3 scaled(const Vector3& value, float amount) {
    return { value.x * amount, value.y * amount, value.z * amount };
}

// Add vector components to combine offsets, movement directions and interpolation contributions.
static Vector3 added(const Vector3& a, const Vector3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

// Rotate a vector around a unit axis by an angle in radians using Rodrigues' rotation formula.
static Vector3 rotateAround(const Vector3& value, const Vector3& axis, float angle) {
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return added(added(scaled(value, cosine), scaled(cross(axis, value), sine)),
        scaled(axis, dot(axis, value) * (1.0f - cosine)));
}

// Generate the maze, initialize floor gravity and timers, and reveal the starting area with the camera
// facing its open passage.
Game::Game(int mazeSize, float seconds, GameMode mode)
    : gameMode(mode), mazeData(mazeSize), px(0), py(StandingEyeHeight), pz(0), lookDirection{ 1, 0, 0 },
    gravityUp{ 0, 1, 0 }, rollFrom{ 0, 1, 0 }, rollAxis{ 1, 0, 0 },
    rollAngle(0), rollTime(0.25f), transitionFromPosition{ 0, StandingEyeHeight, 0 },
    transitionFromDirection{ 1, 0, 0 }, attachedToSurface(false),
    hasWon(false), hasLost(false), timeLeftSec(seconds), elapsedSec(0), points(0) {
    int n = mazeData.size();
    seen.assign(n * n, 0);
    // Face the open neighboring passage so the initial camera looks into the maze.
    lookDirection = (mazeData.cell(0, 1) != '#')
        ? Vector3{ 1, 0, 0 } : Vector3{ 0, 0, 1 };
    reveal();
}

// Convert world X/Z coordinates to a maze cell and treat both wall cells and out-of-bounds cells as
// solid.
bool Game::isWall(float x, float z) const {
    int c = cellIndex(x);
    int r = cellIndex(z);
    return mazeData.cell(r, c) == '#';
}

// Test all four corners of the player's horizontal clearance square against maze walls.
bool Game::canMove(float x, float z) const {
    const float m = PlayerRadius;
    return !isWall(x - m, z - m) && !isWall(x + m, z - m) &&
        !isWall(x - m, z + m) && !isWall(x + m, z + m);
}

// Check that the destination still has a passage ceiling or a neighboring wall supporting the current
// attachment orientation.
bool Game::hasSurfaceSupport(float x, float z) const {
    int r = cellIndex(z);
    int c = cellIndex(x);
    if (std::fabs(gravityUp.y) > 0.5f) return mazeData.cell(r, c) != '#';
    if (gravityUp.x > 0.5f) return mazeData.cell(r, c - 1) == '#';
    if (gravityUp.x < -0.5f) return mazeData.cell(r, c + 1) == '#';
    if (gravityUp.z > 0.5f) return mazeData.cell(r - 1, c) == '#';
    if (gravityUp.z < -0.5f) return mazeData.cell(r + 1, c) == '#';
    return false;
}

// Reset jump momentum, capture the displayed pose, and set the logical destination and rotation used
// by the smooth surface transition.
void Game::beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition,
    bool attached) {
    jumpMovement.reset();
    transitionFromPosition = { px, py, pz };
    transitionFromDirection = lookDirection;
    rollFrom = cameraUp();
    rollTime = 0.0f;
    rollAngle = std::acos(std::clamp(dot(rollFrom, newUp), -1.0f, 1.0f));
    rollAxis = normalized(cross(rollFrom, newUp));
    if (dot(rollAxis, rollAxis) < 0.5f) {
        rollAxis = normalized(cross(lookDirection, rollFrom));
        if (dot(rollAxis, rollAxis) < 0.5f) rollAxis = { 1, 0, 0 };
    }
    lookDirection = normalized(rotateAround(lookDirection, rollAxis, rollAngle));
    gravityUp = newUp;
    px = targetPosition.x;
    py = targetPosition.y;
    pz = targetPosition.z;
    attachedToSurface = attached;
}

// Find the closest exposed wall face or ceiling along the look ray within 2.5 world units, returning
// its contact point and inward normal.
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
        float distance = (CeilingHeight - py) / lookDirection.y;
        float hitX = px + lookDirection.x * distance;
        float hitZ = pz + lookDirection.z * distance;
        if (distance > 0.0f && distance < nearest && mazeData.cell(
            cellIndex(hitZ), cellIndex(hitX)) != '#')
            consider(distance, { 0, -1, 0 }, { hitX, CeilingHeight, hitZ });
    }

    int reach = (int)std::ceil(maxDistance / CellSize) + 1;
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
            if (hitY < 0.0f || hitY > CeilingHeight ||
                hitX < cellCenter(c) - HalfCell || hitX > cellCenter(c) + HalfCell ||
                hitZ < cellCenter(r) - HalfCell || hitZ > cellCenter(r) + HalfCell) return;
            consider(distance, faceNormal, { hitX, hitY, hitZ });
        };

    for (int r = r0; r <= r1; r++) {
        for (int c = c0; c <= c1; c++) {
            float x = cellCenter(c);
            float z = cellCenter(r);
            testWallFace(r, c, r - 1, c, { 0, 0, 1 }, z - HalfCell, false);
            testWallFace(r, c, r + 1, c, { 0, 0, -1 }, z + HalfCell, false);
            testWallFace(r, c, r, c - 1, { 1, 0, 0 }, x - HalfCell, true);
            testWallFace(r, c, r, c + 1, { -1, 0, 0 }, x + HalfCell, true);
        }
    }
    return found;
}

// Offset the player from the contact plane, validate horizontal clearance and support, and begin the
// camera transition when attachment is valid.
bool Game::attachToSurface(const Vector3& normal, const Vector3& contact) {
    Vector3 targetPosition = added(contact, scaled(normal, SurfaceOffset));
    int targetRow = cellIndex(targetPosition.z);
    int targetCol = cellIndex(targetPosition.x);
    if (!canMove(targetPosition.x, targetPosition.z)) return false;

    bool supported = false;
    if (normal.y < -0.5f) supported = mazeData.cell(targetRow, targetCol) != '#';
    else if (normal.x > 0.5f) supported = mazeData.cell(targetRow, targetCol - 1) == '#';
    else if (normal.x < -0.5f) supported = mazeData.cell(targetRow, targetCol + 1) == '#';
    else if (normal.z > 0.5f) supported = mazeData.cell(targetRow - 1, targetCol) == '#';
    else if (normal.z < -0.5f) supported = mazeData.cell(targetRow + 1, targetCol) == '#';
    if (!supported) return false;

    targetPosition.y = normal.y < -0.5f ? CeilingHeight - SurfaceOffset : std::clamp(targetPosition.y, PlayerRadius, CeilingHeight - PlayerRadius);
    beginSurfaceTransition(normal, targetPosition, true);
    return true;
}

// Resolve a cardinal side of the current cell into a wall contact and attempt attachment to that
// neighboring wall.
bool Game::transitionToAdjacentWall(int side) {
    int r = row(), c = col();
    Vector3 normal;
    Vector3 contact{ px, py, pz };
    int wallR = r, wallC = c;
    if (side == 0) {
        wallR--;
        normal = { 0, 0, 1 };
        contact.z = cellCenter(r) - HalfCell;
    }
    else if (side == 1) {
        wallR++;
        normal = { 0, 0, -1 };
        contact.z = cellCenter(r) + HalfCell;
    }
    else if (side == 2) {
        wallC--;
        normal = { 1, 0, 0 };
        contact.x = cellCenter(c) - HalfCell;
    }
    else {
        wallC++;
        normal = { -1, 0, 0 };
        contact.x = cellCenter(c) + HalfCell;
    }
    if (mazeData.cell(wallR, wallC) != '#') return false;
    return attachToSurface(normal, contact);
}

// Choose a supporting wall around the proposed destination, favoring the current orientation when
// distances are similar.
bool Game::transitionToNearestWall(float x, float z) {
    int r = cellIndex(z);
    int c = cellIndex(x);
    if (mazeData.cell(r, c) == '#') return false;

    float bestScore = 1000.0f;
    Vector3 chosenNormal{ 0, 0, 0 };
    Vector3 chosenContact{ 0, 0, 0 };
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

    float clampedX = std::clamp(x, cellCenter(c) - HalfCell, cellCenter(c) + HalfCell);
    float clampedZ = std::clamp(z, cellCenter(r) - HalfCell, cellCenter(r) + HalfCell);
    consider(r - 1, c, { 0, 0, 1 }, { clampedX, py, cellCenter(r) - HalfCell }, std::fabs(z - (cellCenter(r) - HalfCell)));
    consider(r + 1, c, { 0, 0, -1 }, { clampedX, py, cellCenter(r) + HalfCell }, std::fabs(z - (cellCenter(r) + HalfCell)));
    consider(r, c - 1, { 1, 0, 0 }, { cellCenter(c) - HalfCell, py, clampedZ }, std::fabs(x - (cellCenter(c) - HalfCell)));
    consider(r, c + 1, { -1, 0, 0 }, { cellCenter(c) + HalfCell, py, clampedZ }, std::fabs(x - (cellCenter(c) + HalfCell)));
    return found && attachToSurface(chosenNormal, chosenContact);
}

// Return an attached player to floor gravity, or attempt attachment to the wall or ceiling currently
// under the look ray.
void Game::toggleGravity() {
    if (attachedToSurface) {
        beginSurfaceTransition({ 0, 1, 0 }, { px, StandingEyeHeight, pz }, false);
        return;
    }

    Vector3 normal, contact;
    if (findAimedSurface(normal, contact)) attachToSurface(normal, contact);
}

// Reveal the current cell and its surrounding 3-by-3 neighborhood, marking the current cell as
// visited.
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

// Advance an unfinished run's timers, camera orientation and movement, respecting gravity transitions
// and recording an exit-cell win.
void Game::update(const InputState& in) {
    // Ignore further input and timer updates after the run has been won or lost.
    if (hasWon || hasLost) return;

    // Accumulate this frame's duration for the run's elapsed-time result.
    elapsedSec += in.dt;

    // Deduct this frame's duration from the countdown and end the run if no time remains.
    timeLeftSec -= in.dt;
    if (timeLeftSec <= 0.0f) {
        timeLeftSec = 0.0f;
        hasLost = true;
        return;
    }

    rollTime = std::min(rollTime + in.dt, 0.25f);
    if (gameMode == GameMode::ZeroGravity && in.gravityToggle && rollTime >= 0.25f)
        toggleGravity();
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

    if (!attachedToSurface) {
        if (moveJumping(in, movement)) {
            reveal();
            return;
        }
    } else {
        float step = MovementSpeed * in.dt;
        // Split surface movement into steps no larger than 75% of the collision radius to prevent wall
        // tunneling.
        // Resolve X before Z and stop this update when either axis starts a surface transition.
        int substeps = std::max(1, (int)std::ceil(step / (PlayerRadius * 0.75f)));
        float substep = step / substeps;
        for (int part = 0; part < substeps; ++part) {
            if (moveHorizontal(movement.x, substep, true) || moveHorizontal(movement.z, substep, false)) {
                reveal();
                return;
            }
        }

        if (attachedToSurface && std::fabs(gravityUp.y) < 0.5f) {
            float nextY = py + movement.y * step;
            if (nextY < PlayerRadius) {
                beginSurfaceTransition({ 0, 1, 0 }, { px, StandingEyeHeight, pz }, false);
                reveal();
                return;
            }
            if (nextY > CeilingHeight - PlayerRadius) {
                beginSurfaceTransition({ 0, -1, 0 }, { px, CeilingHeight - SurfaceOffset, pz }, true);
                reveal();
                return;
            }
            py = nextY;
        }
        else if (attachedToSurface) {
            py = std::clamp(py + movement.y * step, PlayerRadius, CeilingHeight - PlayerRadius);
        }
    }

    reveal();
    if (mazeData.cell(row(), col()) == 'E') {
        hasWon = true;
        points = mazeData.size() * 100 + (int)std::ceil(timeLeftSec) * 10;
    }
}

// Resolve one horizontal movement axis and return true if collision or lost support starts a surface
// transition.
bool Game::moveHorizontal(float movement, float step, bool alongX) {
    float nextX = alongX ? px + movement * step : px;
    float nextZ = alongX ? pz : pz + movement * step;
    if (canMove(nextX, nextZ)) {
        if (!attachedToSurface || hasSurfaceSupport(nextX, nextZ)) {
            (alongX ? px : pz) = alongX ? nextX : nextZ;
        }
        else if (std::fabs(gravityUp.y) < 0.5f) {
            if (!transitionToNearestWall(nextX, nextZ))
                beginSurfaceTransition({ 0, 1, 0 }, { nextX, StandingEyeHeight, nextZ }, false);
            return true;
        }
    }
    else if (gameMode == GameMode::ZeroGravity && std::fabs(movement) > 0.001f) {
        int side = alongX ? (movement > 0.0f ? 3 : 2) : (movement > 0.0f ? 1 : 0);
        return transitionToAdjacentWall(side);
    }
    return false;
}

// Return the clamped interpolation fraction for the quarter-second surface transition.
float Game::transitionBlend() const {
    return std::clamp(rollTime / 0.25f, 0.0f, 1.0f);
}

// Return the displayed world-space position interpolated from the previous pose to the logical
// transition destination.
Vector3 Game::position() const {
    float blend = transitionBlend();
    return added(scaled(transitionFromPosition, 1.0f - blend), scaled({ px, py, pz }, blend));
}
// Return the displayed X coordinate, including any active surface-transition interpolation.
float Game::x() const { return position().x; }
// Return the displayed eye height, including jump height or an active surface transition.
float Game::y() const { return position().y; }
// Return the displayed Z coordinate, including any active surface-transition interpolation.
float Game::z() const { return position().z; }
// Return the look direction's horizontal angle in radians, measured from +X toward +Z.
float Game::yaw() const { return std::atan2(lookDirection.z, lookDirection.x); }
// Normalize the interpolated look direction so rendering remains smooth during a surface transition.
Vector3 Game::cameraDirection() const {
    float blend = transitionBlend();
    return normalized(added(scaled(transitionFromDirection, 1.0f - blend),
        scaled(lookDirection, blend)));
}
// Rotate the previous up vector through the current transition angle and return the camera's
// normalized up direction.
Vector3 Game::cameraUp() const {
    return normalized(rotateAround(rollFrom, rollAxis, rollAngle * transitionBlend()));
}
// Report whether gravity is attached to a wall or ceiling rather than the floor.
bool Game::surfaceAttached() const { return attachedToSurface; }
// Return the mode selected when this run was created.
GameMode Game::mode() const { return gameMode; }
// Convert the logical Z position to a maze row, independently of camera-transition interpolation.
int Game::row() const { return cellIndex(pz); }
// Convert the logical X position to a maze column, independently of camera-transition interpolation.
int Game::col() const { return cellIndex(px); }

// Read a cell's exploration state, returning unrevealed for coordinates outside the maze.
int Game::seenState(int r, int c) const {
    int n = mazeData.size();
    if (r < 0 || r >= n || c < 0 || c >= n) return 0;
    return seen[r * n + c];
}

// Return the run's remaining countdown in seconds.
float Game::timeLeft() const { return timeLeftSec; }
// Return the accumulated simulation seconds used for completed-run results and best-time records.
float Game::timeTaken() const { return elapsedSec; }
// Return the points assigned on victory, or the initial zero while no win has been recorded.
int Game::score() const { return points; }
// Report whether the logical player position has reached the exit before the countdown expires.
bool Game::won() const { return hasWon; }
// Report whether the run ended because the countdown reached zero.
bool Game::lost() const { return hasLost; }
// Report whether either terminal result has frozen further gameplay updates.
bool Game::finished() const { return hasWon || hasLost; }
// Expose the run's maze as a read-only reference for rendering, animation and route checks.
const MazeData& Game::maze() const { return mazeData; }
