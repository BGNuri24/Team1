#pragma once
#include "../data/GameMode.h"
#include "../data/GravityObstacle.h"
#include "../data/MazeData.h"
#include "../data/MazeGeometry.h"
#include "../data/MazeTheme.h"
#include "JumpMovement.h"
#include "raylib.h"
#include <optional>
#include <vector>

struct InputState {
    // Signed forward input relative to the camera: -1 moves backward, 0 is idle, and +1 moves
    // forward.
    float forward;
    // Signed strafe input relative to the camera: -1 moves left, 0 is idle, and +1 moves right.
    float right;
    float mouseDX, mouseDY;
    float dt;
    bool gravityToggle;
    bool jumpPressed = false;
    bool mapPressed = false;
};

// Distinguish a timed-out run from a lethal barrier collision on the result screen.
enum class FailureReason { None, TimeExpired, ObstacleHit };

class Game {
public:
    // Create a seeded maze run with its selected mode, palette, timer, floor orientation, and
    // starting visibility. Generate hazards only for Zero Gravity and face the camera toward an
    // open starting passage.
    Game(int mazeSize, float seconds, GameMode mode = GameMode::Normal,
         std::optional<std::uint32_t> seed = std::nullopt,
         std::optional<MazeTheme> theme = std::nullopt);
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Game(const Game&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Game& operator=(const Game&) = delete;

    // Advance one unfinished run from a frame's InputState: validate timing, update timers,
    // handle gravity, rotate the view, resolve movement, and reveal cells. Terminal results
    // freeze later updates, and timeout takes priority before movement.
    void update(const InputState& in);

    // Return the X component of the displayed position, including active surface-transition
    // interpolation. Use col() when a logical maze column is needed instead of a rendered
    // coordinate.
    float x() const;
    // Return the displayed eye height in world units, including floor-relative jump height or a
    // surface transition. This is a camera coordinate rather than the player's foot position.
    float y() const;
    // Return the Z component of the displayed position, including active surface-transition
    // interpolation. Use row() when a logical maze row is needed instead of a rendered
    // coordinate.
    float z() const;
    // Return the displayed eye position by interpolating from the captured transition pose to the
    // logical player position. During an ordinary run this matches the current position; reading
    // it does not advance simulation.
    Vector3 position() const;
    // Measure the logical look direction's horizontal angle in radians using atan2, from positive
    // X toward positive Z. This read-only value is useful for map markers and movement-facing
    // checks.
    float yaw() const;
    // Normalize the interpolated look vector used by the renderer during a gravity transfer. The
    // logical direction remains separate from the displayed transition so the camera can rotate
    // smoothly.
    Vector3 cameraDirection() const;
    // Rotate the previous up vector through the current surface-transition angle and normalize
    // it. The renderer uses this orientation to roll between floor and ceiling views.
    Vector3 cameraUp() const;
    // Report whether the logical player is attached to the ceiling. Side-wall collisions never
    // set this flag; an active camera transition may still be interpolating toward that surface.
    bool surfaceAttached() const;
    // Return the game mode selected at construction without changing it. Input handling and
    // rendering use this value to distinguish ordinary runs from Zero Gravity.
    GameMode mode() const;
    // Return the palette selected when this run was constructed. The value stays stable while
    // playing, pausing, and showing results, so rendering cannot reroll the run's appearance.
    MazeTheme theme() const;
    // Return the stored maze-generation seed. Supply this seed, theme, size, and mode when
    // constructing a run to reproduce its layout and visual selection for debugging.
    std::uint32_t seed() const;
    // Convert the logical Z coordinate into a maze row with MazeGeometry::cellIndex. Collision
    // and exit tests use this value independently of the camera's displayed transition position.
    int row() const;
    // Convert the logical X coordinate into a maze column with MazeGeometry::cellIndex. Collision
    // and exit tests use this value independently of the camera's displayed transition position.
    int col() const;
    // Read a cell's exploration state: 0 is unrevealed, 1 is revealed, and 2 is visited.
    // Out-of-bounds coordinates return 0 instead of indexing outside the visibility vector.
    int seenState(int r, int c) const;
    // Return the remaining countdown in seconds; expiry clamps it to zero. This getter never
    // advances time, so drawing a paused or completed run leaves the timer unchanged.
    float timeLeft() const;
    // Return the simulation seconds accumulated during active updates for this run. Pausing and
    // terminal results stop accumulation; completed wins use this value for best-time
    // comparisons.
    float timeTaken() const;
    // Report whether movement reached the exit before the run failed. This reads the terminal
    // result and does not recheck collision, update timers, or write a best-time record.
    bool won() const;
    // Report whether the run ended because its timer expired or an obstacle was hit. Call
    // failureReason() when the end screen needs the specific cause.
    bool lost() const;
    // Return the stored loss cause so results can distinguish a timeout from lethal obstacle
    // contact. FailureReason::None represents a run without a recorded failure.
    FailureReason failureReason() const;
    // Borrow the run's immutable hazard layout for rendering and route checks. Normal mode leaves
    // this vector empty; callers must not retain the reference after the Game is destroyed.
    const std::vector<GravityObstacle>& obstacles() const;
    // Report whether either win or loss has become terminal. The application uses this to open
    // results, while Game::update stops accepting further simulation changes.
    bool finished() const;
    // Borrow the run's maze for rendering, generation preview, and route validation without
    // copying its grid. The reference remains valid only while this Game exists.
    const MazeData& maze() const;

private:
    // Advance floor-relative jump and horizontal momentum in intervals no larger than 1/120
    // second, checking vertical and horizontal hazard motion each time. Clear velocity blocked by
    // walls; return true when a lethal collision stops the update.
    bool moveJumping(const InputState& input, const Vector3& wish);
    JumpMovement jumpMovement;
    // Return a fraction from zero to one for the current quarter-second surface transfer. The
    // clamped result is shared by displayed position and camera rotation interpolation.
    float transitionBlend() const;
    // Try one X- or Z-axis displacement after checking wall clearance, sweeping the accepted
    // segment against hazards before committing it. Return true for lethal obstacle contact so
    // the caller stops further movement immediately.
    bool moveHorizontal(float movement, float step, bool alongX);
    // Convert world-space X/Z coordinates to a maze column and row and test for '#'. MazeData
    // treats out-of-range cells as walls, so the same test enforces the outer boundary.
    bool isWall(float x, float z) const;
    // Test the four corners of the player's horizontal clearance square against maze walls. This
    // checks a proposed X/Z position without committing movement or evaluating lethal obstacle
    // volumes.
    bool canMove(float x, float z) const;
    // Reset jump momentum, capture the displayed pose, and set the logical destination and new
    // gravity orientation. Rendering interpolates from the captured pose over the quarter-second
    // transition.
    void beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition, bool attached);
    // Request a floor-to-ceiling or ceiling-to-floor transfer while preserving X/Z position and
    // corridor heading. Sweep the player's body through the transfer first so a barrier can cause
    // failure before the new surface is accepted.
    void toggleGravity();
    // Sweep the player's body from the previous pose to the proposed pose with each endpoint's
    // surface orientation. On contact, set the terminal loss and ObstacleHit reason; return
    // whether this movement caused failure.
    bool checkObstacleCollision(Vector3 from, Vector3 to, bool fromCeiling, bool toCeiling);
    // Mark the logical current cell as visited and expose its surrounding 3-by-3 neighborhood.
    // Preserve previously visited cells and leave out-of-bounds coordinates outside the
    // exploration vector.
    void reveal();

    GameMode gameMode;
    std::uint32_t runSeed;
    MazeTheme mazeTheme;
    MazeData mazeData;
    std::vector<GravityObstacle> gravityObstacles;
    FailureReason failure = FailureReason::None;
    float px, py, pz;
    Vector3 lookDirection;
    Vector3 gravityUp;
    Vector3 rollFrom;
    Vector3 rollAxis;
    float rollAngle;
    float rollTime;
    Vector3 transitionFromPosition;
    Vector3 transitionFromDirection;
    bool attachedToSurface;
    bool hasWon;
    bool hasLost;
    float timeLeftSec;
    float elapsedSec;
    std::vector<char> seen;
};
