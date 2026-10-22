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
        // face the open direction at the start
        ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
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
    if (gravityUp.y < -0.5f) return mazeData.cell(r, c) != '#';
    if (gravityUp.y > 0.5f) return mazeData.cell(r, c) != '#';
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
