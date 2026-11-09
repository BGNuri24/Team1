#include "Game.h"
#include "GravityObstacles.h"
#include <algorithm>
#include <cmath>

using namespace MazeGeometry;

// Compute the scalar product of two vectors for projections, alignment tests, and rotation
// calculations. Unit vectors produce the cosine of their angle; this helper does not normalize
// its inputs.
static float dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Compute the right-handed vector perpendicular to the two inputs. Camera-right directions and
// surface-rotation axes use this result, which callers normalize when they need a unit vector.
static Vector3 cross(const Vector3& a, const Vector3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// Return a vector with unit length while preserving its direction. Treat a near-zero magnitude as
// a zero vector to avoid division by a very small value during camera and movement calculations.
static Vector3 normalized(const Vector3& value) {
    float length = std::sqrt(dot(value, value));
    if (length < 0.0001f)
        return {0, 0, 0};
    return {value.x / length, value.y / length, value.z / length};
}

// Multiply all three vector components by amount and return the result without changing the
// input. Positive factors preserve direction; negative factors reverse it.
static Vector3 scaled(const Vector3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

// Add the corresponding components of two vectors without modifying either input. Movement
// offsets and rotation interpolation combine their vector contributions through this helper.
static Vector3 added(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

// Apply Rodrigues' rotation formula to rotate a vector about a unit axis by angle radians.
// Callers supply the normalized axis; the helper preserves the input vector's length within
// floating-point precision.
static Vector3 rotateAround(const Vector3& value, const Vector3& axis, float angle) {
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    return added(added(scaled(value, cosine), scaled(cross(axis, value), sine)),
                 scaled(axis, dot(axis, value) * (1.0f - cosine)));
}

// Create a seeded maze run with its selected mode, palette, timer, floor orientation, and
// starting visibility. Generate hazards only for Zero Gravity and face the camera toward an open
// starting passage.
Game::Game(int mazeSize, float seconds, GameMode mode, std::optional<std::uint32_t> seed,
           std::optional<MazeTheme> theme)
    : gameMode(mode), runSeed(seed ? *seed : randomSeed()),
      mazeTheme(theme.value_or(ActiveMazeTheme)), mazeData(mazeSize, runSeed), px(0),
      py(StandingEyeHeight), pz(0), lookDirection{1, 0, 0}, gravityUp{0, 1, 0}, rollFrom{0, 1, 0},
      rollAxis{1, 0, 0}, rollAngle(0), rollTime(0.25f),
      transitionFromPosition{0, StandingEyeHeight, 0}, transitionFromDirection{1, 0, 0},
      attachedToSurface(false), hasWon(false), hasLost(false), timeLeftSec(seconds), elapsedSec(0) {
    int n = mazeData.size();
    seen.assign(n * n, 0);
    if (gameMode == GameMode::ZeroGravity)
        gravityObstacles = generateGravityObstacles(mazeData, runSeed);
    // Face the open neighboring passage so the initial camera looks into the maze.
    lookDirection = (mazeData.cell(0, 1) != '#') ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
    reveal();
}

// Convert world-space X/Z coordinates to a maze column and row and test for '#'. MazeData treats
// out-of-range cells as walls, so the same test enforces the outer boundary.
bool Game::isWall(float x, float z) const {
    int c = cellIndex(x);
    int r = cellIndex(z);
    return mazeData.cell(r, c) == '#';
}

// Test the four corners of the player's horizontal clearance square against maze walls. This
// checks a proposed X/Z position without committing movement or evaluating lethal obstacle
// volumes.
bool Game::canMove(float x, float z) const {
    const float m = PlayerRadius;
    return !isWall(x - m, z - m) && !isWall(x + m, z - m) && !isWall(x - m, z + m) &&
           !isWall(x + m, z + m);
}

// Reset jump momentum, capture the displayed pose, and set the logical destination and new
// gravity orientation. Rendering interpolates from the captured pose over the quarter-second
// transition.
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

// Request a floor-to-ceiling or ceiling-to-floor transfer while preserving X/Z position and
// corridor heading. Sweep the player's body through the transfer first so a barrier can cause
// failure before the new surface is accepted.
void Game::toggleGravity() {
    const bool ceiling = !attachedToSurface;
    const Vector3 target = {px, ceiling ? CeilingHeight - StandingEyeHeight : StandingEyeHeight,
                            pz};
    if (checkObstacleCollision({px, py, pz}, target, attachedToSurface, ceiling))
        return;
    beginSurfaceTransition(ceiling ? Vector3{0, -1, 0} : Vector3{0, 1, 0}, target, ceiling);
}

// Mark the logical current cell as visited and expose its surrounding 3-by-3 neighborhood.
// Preserve previously visited cells and leave out-of-bounds coordinates outside the exploration
// vector.
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

// Advance one unfinished run from a frame's InputState: validate timing, update timers, handle
// gravity, rotate the view, resolve movement, and reveal cells. Terminal results freeze later
// updates, and timeout takes priority before movement.
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
    if (mazeData.cell(row(), col()) == 'E')
        hasWon = true;
}

// Try one X- or Z-axis displacement after checking wall clearance, sweeping the accepted segment
// against hazards before committing it. Return true for lethal obstacle contact so the caller
// stops further movement immediately.
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

// Sweep the player's body from the previous pose to the proposed pose with each endpoint's
// surface orientation. On contact, set the terminal loss and ObstacleHit reason; return whether
// this movement caused failure.
bool Game::checkObstacleCollision(Vector3 from, Vector3 to, bool fromCeiling, bool toCeiling) {
    if (!hitsGravityObstacle(gravityObstacles, from, fromCeiling, to, toCeiling))
        return false;
    hasLost = true;
    failure = FailureReason::ObstacleHit;
    return true;
}

// Return a fraction from zero to one for the current quarter-second surface transfer. The clamped
// result is shared by displayed position and camera rotation interpolation.
float Game::transitionBlend() const {
    return std::clamp(rollTime / 0.25f, 0.0f, 1.0f);
}

// Return the displayed eye position by interpolating from the captured transition pose to the
// logical player position. During an ordinary run this matches the current position; reading it
// does not advance simulation.
Vector3 Game::position() const {
    float blend = transitionBlend();
    return added(scaled(transitionFromPosition, 1.0f - blend), scaled({px, py, pz}, blend));
}
// Return the X component of the displayed position, including active surface-transition
// interpolation. Use col() when a logical maze column is needed instead of a rendered coordinate.
float Game::x() const {
    return position().x;
}
// Return the displayed eye height in world units, including floor-relative jump height or a
// surface transition. This is a camera coordinate rather than the player's foot position.
float Game::y() const {
    return position().y;
}
// Return the Z component of the displayed position, including active surface-transition
// interpolation. Use row() when a logical maze row is needed instead of a rendered coordinate.
float Game::z() const {
    return position().z;
}
// Measure the logical look direction's horizontal angle in radians using atan2, from positive X
// toward positive Z. This read-only value is useful for map markers and movement-facing checks.
float Game::yaw() const {
    return std::atan2(lookDirection.z, lookDirection.x);
}
// Normalize the interpolated look vector used by the renderer during a gravity transfer. The
// logical direction remains separate from the displayed transition so the camera can rotate
// smoothly.
Vector3 Game::cameraDirection() const {
    float blend = transitionBlend();
    return normalized(
        added(scaled(transitionFromDirection, 1.0f - blend), scaled(lookDirection, blend)));
}
// Rotate the previous up vector through the current surface-transition angle and normalize it.
// The renderer uses this orientation to roll between floor and ceiling views.
Vector3 Game::cameraUp() const {
    return normalized(rotateAround(rollFrom, rollAxis, rollAngle * transitionBlend()));
}
// Report whether the logical player is attached to the ceiling. Side-wall collisions never set
// this flag; an active camera transition may still be interpolating toward that surface.
bool Game::surfaceAttached() const {
    return attachedToSurface;
}
// Return the game mode selected at construction without changing it. Input handling and rendering
// use this value to distinguish ordinary runs from Zero Gravity.
GameMode Game::mode() const {
    return gameMode;
}
// Return the palette selected when this run was constructed. The value stays stable while
// playing, pausing, and showing results, so rendering cannot reroll the run's appearance.
MazeTheme Game::theme() const {
    return mazeTheme;
}
// Return the stored maze-generation seed. Supply this seed, theme, size, and mode when
// constructing a run to reproduce its layout and visual selection for debugging.
std::uint32_t Game::seed() const {
    return runSeed;
}
// Convert the logical Z coordinate into a maze row with MazeGeometry::cellIndex. Collision and
// exit tests use this value independently of the camera's displayed transition position.
int Game::row() const {
    return cellIndex(pz);
}
// Convert the logical X coordinate into a maze column with MazeGeometry::cellIndex. Collision and
// exit tests use this value independently of the camera's displayed transition position.
int Game::col() const {
    return cellIndex(px);
}

// Read a cell's exploration state: 0 is unrevealed, 1 is revealed, and 2 is visited.
// Out-of-bounds coordinates return 0 instead of indexing outside the visibility vector.
int Game::seenState(int r, int c) const {
    int n = mazeData.size();
    if (r < 0 || r >= n || c < 0 || c >= n)
        return 0;
    return seen[r * n + c];
}

// Return the remaining countdown in seconds; expiry clamps it to zero. This getter never advances
// time, so drawing a paused or completed run leaves the timer unchanged.
float Game::timeLeft() const {
    return timeLeftSec;
}
// Return the simulation seconds accumulated during active updates for this run. Pausing and
// terminal results stop accumulation; completed wins use this value for best-time comparisons.
float Game::timeTaken() const {
    return elapsedSec;
}
// Report whether movement reached the exit before the run failed. This reads the terminal result
// and does not recheck collision, update timers, or write a best-time record.
bool Game::won() const {
    return hasWon;
}
// Report whether the run ended because its timer expired or an obstacle was hit. Call
// failureReason() when the end screen needs the specific cause.
bool Game::lost() const {
    return hasLost;
}
// Return the stored loss cause so results can distinguish a timeout from lethal obstacle contact.
// FailureReason::None represents a run without a recorded failure.
FailureReason Game::failureReason() const {
    return failure;
}

// Borrow the run's immutable hazard layout for rendering and route checks. Normal mode leaves
// this vector empty; callers must not retain the reference after the Game is destroyed.
const std::vector<GravityObstacle>& Game::obstacles() const {
    return gravityObstacles;
}

// Report whether either win or loss has become terminal. The application uses this to open
// results, while Game::update stops accepting further simulation changes.
bool Game::finished() const {
    return hasWon || hasLost;
}
// Borrow the run's maze for rendering, generation preview, and route validation without copying
// its grid. The reference remains valid only while this Game exists.
const MazeData& Game::maze() const {
    return mazeData;
}
