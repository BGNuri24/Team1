#include "Game.h"
#include "GravityObstacles.h"
#include <algorithm>
#include <cmath>

using namespace MazeGeometry;

// Compute the scalar product used to project movement, measure alignment and recover rotation
// angles.
static float dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Compute a right-handed perpendicular vector for camera axes and surface-transition rotation axes.
static Vector3 cross(const Vector3& a, const Vector3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Return a unit-length direction, or a zero vector when the input is too short to normalize safely.
static Vector3 normalized(const Vector3& value) {
    float length = std::sqrt(dot(value, value));
    if (length < 0.0001f)
        return {0, 0, 0};
    return {value.x / length, value.y / length, value.z / length};
}

// Multiply every vector component by the same scalar without changing its direction for a positive
// factor.
static Vector3 scaled(const Vector3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

// Add vector components to combine offsets, movement directions and interpolation contributions.
static Vector3 added(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

// Rotate a vector around a unit axis by an angle in radians using Rodrigues' rotation formula.
static Vector3 rotateAround(const Vector3& value, const Vector3& axis, float angle) {
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return added(added(scaled(value, cosine), scaled(cross(axis, value), sine)),
                 scaled(axis, dot(axis, value) * (1.0f - cosine)));
}

// Generate the maze, initialize floor gravity and timers, and reveal the starting area with the
// camera facing its open passage.
Game::Game(int mazeSize, float seconds, GameMode mode, std::optional<std::uint32_t> seed,
           std::optional<MazeTheme> theme)
    : gameMode(mode), runSeed(seed ? *seed : randomSeed()),
      mazeTheme(theme.value_or(ActiveMazeTheme)), mazeData(mazeSize, runSeed), px(0),
      py(StandingEyeHeight), pz(0), lookDirection{1, 0, 0}, gravityUp{0, 1, 0}, rollFrom{0, 1, 0},
      rollAxis{1, 0, 0}, rollAngle(0), rollTime(0.25f),
      transitionFromPosition{0, StandingEyeHeight, 0}, transitionFromDirection{1, 0, 0},
      attachedToSurface(false), hasWon(false), hasLost(false), timeLeftSec(seconds), elapsedSec(0),
      points(0) {
    int n = mazeData.size();
    seen.assign(n * n, 0);
    if (gameMode == GameMode::ZeroGravity)
        gravityObstacles = generateGravityObstacles(mazeData, runSeed);
    // Face the open neighboring passage so the initial camera looks into the maze.
    lookDirection = (mazeData.cell(0, 1) != '#') ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
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
    return !isWall(x - m, z - m) && !isWall(x + m, z - m) && !isWall(x - m, z + m) &&
           !isWall(x + m, z + m);
}

// Reset jump momentum, capture the displayed pose, and set the logical destination and rotation
// used by the smooth surface transition.
void Game::beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition,
                                  bool attached) {
    jumpMovement.reset();
    transitionFromPosition = {px, py, pz};
    transitionFromDirection = lookDirection;
    rollFrom = cameraUp();
    rollTime = 0.0f;
    rollAngle = std::acos(std::clamp(dot(rollFrom, newUp), -1.0f, 1.0f));
    rollAxis = normalized(cross(rollFrom, newUp));
    if (dot(rollAxis, rollAxis) < 0.5f) {
        // Roll around the corridor heading during a half-turn, preserving forward travel.
        rollAxis =
            normalized(added(lookDirection, scaled(rollFrom, -dot(lookDirection, rollFrom))));
        if (dot(rollAxis, rollAxis) < 0.5f)
            rollAxis = {1, 0, 0};
    }
    lookDirection = normalized(rotateAround(lookDirection, rollAxis, rollAngle));
    gravityUp = newUp;
    px = targetPosition.x;
    py = targetPosition.y;
    pz = targetPosition.z;
    attachedToSurface = attached;
}

// Switch directly between floor and ceiling without aiming or changing the player's X/Z position.
// Sweep the whole transfer first so flipping through a nearby barrier is still lethal.
void Game::toggleGravity() {
    const bool ceiling = !attachedToSurface;
    const Vector3 target = {px, ceiling ? CeilingHeight - StandingEyeHeight : StandingEyeHeight,
                            pz};
    if (checkObstacleCollision({px, py, pz}, target, attachedToSurface, ceiling))
        return;
    beginSurfaceTransition(ceiling ? Vector3{0, -1, 0} : Vector3{0, 1, 0}, target, ceiling);
}

// Reveal the current cell and its surrounding 3-by-3 neighborhood, marking the current cell as
// visited.
void Game::reveal() {
    int n = mazeData.size();
    int r = row(), c = col();
    if (r < 0 || r >= n || c < 0 || c >= n)
        return;
    for (int i = r - 1; i <= r + 1; i++)
        for (int j = c - 1; j <= c + 1; j++)
            if (i >= 0 && i < n && j >= 0 && j < n && seen[i * n + j] == 0)
                seen[i * n + j] = 1;
    seen[r * n + c] = 2;
}

// Advance an unfinished run's timers, camera orientation and movement, respecting gravity
// transitions and recording an exit-cell win.
void Game::update(const InputState& in) {
    // Ignore further input and timer updates after the run has been won or lost.
    if (hasWon || hasLost)
        return;
    if (!std::isfinite(in.dt) || in.dt < 0.0f)
        return;

    // Accumulate this frame's duration for the run's elapsed-time result.
    elapsedSec += in.dt;

    // Deduct this frame's duration from the countdown and end the run if no time remains.
    timeLeftSec -= in.dt;
    if (timeLeftSec <= 0.0f) {
        timeLeftSec = 0.0f;
        hasLost = true;
        failure = FailureReason::TimeExpired;
        return;
    }

    rollTime = std::min(rollTime + in.dt, 0.25f);
    if (gameMode == GameMode::ZeroGravity && in.gravityToggle && rollTime >= 0.25f)
        toggleGravity();
    if (hasLost)
        return;
    if (rollTime < 0.25f) {
        reveal();
        return;
    }

    lookDirection = normalized(rotateAround(lookDirection, gravityUp, -in.mouseDX * 0.0025f));
    Vector3 cameraRight = normalized(cross(lookDirection, gravityUp));
    float currentPitch = std::asin(std::clamp(dot(lookDirection, gravityUp), -1.0f, 1.0f));
    float targetPitch = std::clamp(currentPitch - in.mouseDY * 0.0025f, -1.5f, 1.5f);
    lookDirection =
        normalized(rotateAround(lookDirection, cameraRight, targetPitch - currentPitch));
    cameraRight = normalized(cross(lookDirection, gravityUp));

    Vector3 moveForward =
        normalized(added(lookDirection, scaled(gravityUp, -dot(lookDirection, gravityUp))));
    Vector3 movement = added(scaled(moveForward, in.forward), scaled(cameraRight, in.right));
    float movementLength = std::sqrt(dot(movement, movement));
    if (movementLength > 1.0f)
        movement = scaled(movement, 1.0f / movementLength);

    if (!attachedToSurface) {
        if (moveJumping(in, movement)) {
            reveal();
            return;
        }
    } else {
        float step = MovementSpeed * in.dt;
        // Split surface movement into steps no larger than 75% of the collision radius to prevent
        // wall tunneling. Resolve X before Z and stop immediately if either axis hits a barrier.
        int substeps = std::max(1, (int)std::ceil(step / (PlayerRadius * 0.75f)));
        float substep = step / substeps;
        for (int part = 0; part < substeps; ++part) {
            if (moveHorizontal(movement.x, substep, true) ||
                moveHorizontal(movement.z, substep, false)) {
                reveal();
                return;
            }
        }
    }

    reveal();
    if (mazeData.cell(row(), col()) == 'E') {
        hasWon = true;
        points = mazeData.size() * 100 + (int)std::ceil(timeLeftSec) * 10;
    }
}

// Resolve horizontal clearance without ever attaching to a side wall. Test each accepted segment
// against hazards before committing the destination, so the player stops at the failed approach.
bool Game::moveHorizontal(float movement, float step, bool alongX) {
    const float nextX = alongX ? px + movement * step : px;
    const float nextZ = alongX ? pz : pz + movement * step;
    if (canMove(nextX, nextZ)) {
        if (checkObstacleCollision({px, py, pz}, {nextX, py, nextZ}, attachedToSurface,
                                   attachedToSurface))
            return true;
        px = nextX;
        pz = nextZ;
    }
    return false;
}

// Freeze an obstacle loss through the same finished-run state used by the existing end-screen flow.
bool Game::checkObstacleCollision(Vector3 from, Vector3 to, bool fromCeiling, bool toCeiling) {
    if (!hitsGravityObstacle(gravityObstacles, from, fromCeiling, to, toCeiling))
        return false;
    hasLost = true;
    failure = FailureReason::ObstacleHit;
    return true;
}

// Return the clamped interpolation fraction for the quarter-second surface transition.
float Game::transitionBlend() const {
    return std::clamp(rollTime / 0.25f, 0.0f, 1.0f);
}

// Return the displayed world-space position interpolated from the previous pose to the logical
// transition destination.
Vector3 Game::position() const {
    float blend = transitionBlend();
    return added(scaled(transitionFromPosition, 1.0f - blend), scaled({px, py, pz}, blend));
}
// Return the displayed X coordinate, including any active surface-transition interpolation.
float Game::x() const {
    return position().x;
}
// Return the displayed eye height, including jump height or an active surface transition.
float Game::y() const {
    return position().y;
}
// Return the displayed Z coordinate, including any active surface-transition interpolation.
float Game::z() const {
    return position().z;
}
// Return the look direction's horizontal angle in radians, measured from +X toward +Z.
float Game::yaw() const {
    return std::atan2(lookDirection.z, lookDirection.x);
}
// Normalize the interpolated look direction so rendering remains smooth during a surface
// transition.
Vector3 Game::cameraDirection() const {
    float blend = transitionBlend();
    return normalized(
        added(scaled(transitionFromDirection, 1.0f - blend), scaled(lookDirection, blend)));
}
// Rotate the previous up vector through the current transition angle and return the camera's
// normalized up direction.
Vector3 Game::cameraUp() const {
    return normalized(rotateAround(rollFrom, rollAxis, rollAngle * transitionBlend()));
}
// Report whether the player is walking on the ceiling rather than the floor.
bool Game::surfaceAttached() const {
    return attachedToSurface;
}
// Return the mode selected when this run was created.
GameMode Game::mode() const {
    return gameMode;
}
// Expose the run's immutable theme without rerolling it during rendering, pausing or results.
MazeTheme Game::theme() const {
    return mazeTheme;
}
// Expose the stored seed rather than relying on process-wide random-generator state for
// reproduction.
std::uint32_t Game::seed() const {
    return runSeed;
}
// Convert the logical Z position to a maze row, independently of camera-transition interpolation.
int Game::row() const {
    return cellIndex(pz);
}
// Convert the logical X position to a maze column, independently of camera-transition
// interpolation.
int Game::col() const {
    return cellIndex(px);
}

// Read a cell's exploration state, returning unrevealed for coordinates outside the maze.
int Game::seenState(int r, int c) const {
    int n = mazeData.size();
    if (r < 0 || r >= n || c < 0 || c >= n)
        return 0;
    return seen[r * n + c];
}

// Return the run's remaining countdown in seconds.
float Game::timeLeft() const {
    return timeLeftSec;
}
// Return the accumulated simulation seconds used for completed-run results and best-time records.
float Game::timeTaken() const {
    return elapsedSec;
}
// Return the points assigned on victory, or the initial zero while no win has been recorded.
int Game::score() const {
    return points;
}
// Report whether the logical player position has reached the exit before the countdown expires.
bool Game::won() const {
    return hasWon;
}
// Report whether the run ended because of a timeout or an obstacle collision.
bool Game::lost() const {
    return hasLost;
}
// Expose the failure cause so the report never describes a barrier collision as a timeout.
FailureReason Game::failureReason() const {
    return failure;
}

// Expose the immutable run layout to rendering and route validation.
const std::vector<GravityObstacle>& Game::obstacles() const {
    return gravityObstacles;
}

// Report whether either terminal result has frozen further gameplay updates.
bool Game::finished() const {
    return hasWon || hasLost;
}
// Expose the run's maze as a read-only reference for rendering, animation and route checks.
const MazeData& Game::maze() const {
    return mazeData;
}
