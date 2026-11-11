#include "Game.h"
#include "GravityObstacles.h"
#include <algorithm>
#include <cmath>

using namespace MazeGeometry;

// Compute the scalar product of two vectors for projections, alignment tests, and rotation
// calculations. Unit vectors produce the cosine of their angle; this helper does not normalize
// its inputs.
static float Dot(const Vector3& CurrentA, const Vector3& B) {
    return CurrentA.x * B.x + CurrentA.y * B.y + CurrentA.z * B.z;
}

// Compute the right-handed vector perpendicular to the two inputs. Camera-right directions and
// surface-rotation axes use this result, which callers normalize when they need a unit vector.
static Vector3 Cross(const Vector3& CurrentA, const Vector3& B) {
    return {CurrentA.y * B.z - CurrentA.z * B.y, CurrentA.z * B.x - CurrentA.x * B.z, CurrentA.x * B.y - CurrentA.y * B.x};
}

// Return a vector with unit length while preserving its direction. Treat a near-zero magnitude as
// a zero vector to avoid division by a very small value during camera and movement calculations.
static Vector3 Normalized(const Vector3& Value) {
    float Length = std::sqrt(Dot(Value, Value));
    if (Length < 0.0001f)
        return {0, 0, 0};
    return {Value.x / Length, Value.y / Length, Value.z / Length};
}

// Multiply all three vector components by amount and return the result without changing the
// input. Positive factors preserve direction; negative factors reverse it.
static Vector3 Scaled(const Vector3& Value, float Amount) {
    return {Value.x * Amount, Value.y * Amount, Value.z * Amount};
}

// Add the corresponding components of two vectors without modifying either input. Movement
// offsets and rotation interpolation combine their vector contributions through this helper.
static Vector3 Added(const Vector3& CurrentA, const Vector3& B) {
    return {CurrentA.x + B.x, CurrentA.y + B.y, CurrentA.z + B.z};
}

// Apply Rodrigues' rotation formula to rotate a vector about a unit axis by angle radians.
// Callers supply the normalized axis; the helper preserves the input vector's length within
// floating-point precision.
static Vector3 RotateAround(const Vector3& Value, const Vector3& Axis, float Angle) {
    float Cosine = std::cos(Angle);
    float Sine = std::sin(Angle);
    return Added(Added(Scaled(Value, Cosine), Scaled(Cross(Axis, Value), Sine)),
                 Scaled(Axis, Dot(Axis, Value) * (1.0f - Cosine)));
}

// Create a seeded maze run with its selected mode, palette, timer, floor orientation, and
// starting visibility. Generate hazards only for Zero Gravity and face the camera toward an open
// starting passage.
Game::Game(int MazeSize, float Seconds, GameMode Mode, std::optional<std::uint32_t> CurrentSeed,
           std::optional<MazeTheme> Theme)
    : CurrentGameMode(Mode), RunSeed(CurrentSeed ? *CurrentSeed : RandomSeed()),
      CurrentMazeTheme(Theme.value_or(ActiveMazeTheme)), CurrentMazeData(MazeSize, RunSeed), Px(0),
      Py(StandingEyeHeight), Pz(0), LookDirection{1, 0, 0}, GravityUp{0, 1, 0}, RollFrom{0, 1, 0},
      RollAxis{1, 0, 0}, RollAngle(0), RollTime(0.25f),
      TransitionFromPosition{0, StandingEyeHeight, 0}, TransitionFromDirection{1, 0, 0},
      AttachedToSurface(false), HasWon(false), HasLost(false), TimeLeftSec(Seconds), ElapsedSec(0) {
    int N = CurrentMazeData.Size();
    Seen.assign(N * N, 0);
    if (CurrentGameMode == GameMode::ZeroGravity)
        CurrentGravityObstacles = GenerateGravityObstacles(CurrentMazeData, RunSeed);
    // Face the open neighboring passage so the initial camera looks into the maze.
    LookDirection = (CurrentMazeData.Cell(0, 1) != '#') ? Vector3{1, 0, 0} : Vector3{0, 0, 1};
    Reveal();
}

// Convert world-space X/Z coordinates to a maze column and row and test for '#'. MazeData treats
// out-of-range cells as walls, so the same test enforces the outer boundary.
bool Game::IsWall(float CurrentX, float CurrentZ) const {
    int CurrentC = CellIndex(CurrentX);
    int CurrentR = CellIndex(CurrentZ);
    return CurrentMazeData.Cell(CurrentR, CurrentC) == '#';
}

// Test the four corners of the player's horizontal clearance square against maze walls. This
// checks a proposed X/Z position without committing movement or evaluating lethal obstacle
// volumes.
bool Game::CanMove(float CurrentX, float CurrentZ) const {
    const float CurrentM = PlayerRadius;
    return !IsWall(CurrentX - CurrentM, CurrentZ - CurrentM) && !IsWall(CurrentX + CurrentM, CurrentZ - CurrentM) && !IsWall(CurrentX - CurrentM, CurrentZ + CurrentM) &&
           !IsWall(CurrentX + CurrentM, CurrentZ + CurrentM);
}

// Reset jump momentum, capture the displayed pose, and set the logical destination and new
// gravity orientation. Rendering interpolates from the captured pose over the quarter-second
// transition.
void Game::BeginSurfaceTransition(const Vector3& NewUp, const Vector3& TargetPosition,
                                  bool Attached) {
    CurrentJumpMovement.Reset();
    TransitionFromPosition = {Px, Py, Pz};
    TransitionFromDirection = LookDirection;
    RollFrom = CameraUp();
    RollTime = 0.0f;
    RollAngle = std::acos(std::clamp(Dot(RollFrom, NewUp), -1.0f, 1.0f));
    RollAxis = Normalized(Cross(RollFrom, NewUp));
    if (Dot(RollAxis, RollAxis) < 0.5f) {
        // Roll around the corridor heading during a half-turn, preserving forward travel.
        RollAxis =
            Normalized(Added(LookDirection, Scaled(RollFrom, -Dot(LookDirection, RollFrom))));
        if (Dot(RollAxis, RollAxis) < 0.5f)
            RollAxis = {1, 0, 0};
    }
    LookDirection = Normalized(RotateAround(LookDirection, RollAxis, RollAngle));
    GravityUp = NewUp;
    Px = TargetPosition.x;
    Py = TargetPosition.y;
    Pz = TargetPosition.z;
    AttachedToSurface = Attached;
}

// Request a floor-to-ceiling or ceiling-to-floor transfer while preserving X/Z position and
// corridor heading. Sweep the player's body through the transfer first so a barrier can cause
// failure before the new surface is accepted.
void Game::ToggleGravity() {
    const bool Ceiling = !AttachedToSurface;
    const Vector3 Target = {Px, Ceiling ? CeilingHeight - StandingEyeHeight : StandingEyeHeight,
                            Pz};
    if (CheckObstacleCollision({Px, Py, Pz}, Target, AttachedToSurface, Ceiling))
        return;
    BeginSurfaceTransition(Ceiling ? Vector3{0, -1, 0} : Vector3{0, 1, 0}, Target, Ceiling);
}

// Mark the logical current cell as visited and expose its surrounding 3-by-3 neighborhood.
// Preserve previously visited cells and leave out-of-bounds coordinates outside the exploration
// vector.
void Game::Reveal() {
    int N = CurrentMazeData.Size();
    int CurrentR = Row(), CurrentC = Col();
    if (CurrentR < 0 || CurrentR >= N || CurrentC < 0 || CurrentC >= N)
        return;
    for (int I = CurrentR - 1; I <= CurrentR + 1; I++)
        for (int J = CurrentC - 1; J <= CurrentC + 1; J++)
            if (I >= 0 && I < N && J >= 0 && J < N && Seen[I * N + J] == 0)
                Seen[I * N + J] = 1;
    Seen[CurrentR * N + CurrentC] = 2;
}

// Advance one unfinished run from a frame's InputState: validate timing, update timers, handle
// gravity, rotate the view, resolve movement, and reveal cells. Terminal results freeze later
// updates, and timeout takes priority before movement.
void Game::Update(const InputState& In) {
    // Ignore further input and timer updates after the run has been won or lost.
    if (HasWon || HasLost)
        return;
    if (!std::isfinite(In.Dt) || In.Dt < 0.0f)
        return;

    // Accumulate this frame's duration for the run's elapsed-time result.
    ElapsedSec += In.Dt;

    // Deduct this frame's duration from the countdown and end the run if no time remains.
    TimeLeftSec -= In.Dt;
    if (TimeLeftSec <= 0.0f) {
        TimeLeftSec = 0.0f;
        HasLost = true;
        CurrentFailure = FailureReason::TimeExpired;
        return;
    }

    RollTime = std::min(RollTime + In.Dt, 0.25f);
    if (CurrentGameMode == GameMode::ZeroGravity && In.GravityToggle && RollTime >= 0.25f)
        ToggleGravity();
    if (HasLost)
        return;
    if (RollTime < 0.25f) {
        Reveal();
        return;
    }

    LookDirection = Normalized(RotateAround(LookDirection, GravityUp, -In.MouseDx * 0.0025f));
    Vector3 CameraRight = Normalized(Cross(LookDirection, GravityUp));
    float CurrentPitch = std::asin(std::clamp(Dot(LookDirection, GravityUp), -1.0f, 1.0f));
    float TargetPitch = std::clamp(CurrentPitch - In.MouseDy * 0.0025f, -1.5f, 1.5f);
    LookDirection =
        Normalized(RotateAround(LookDirection, CameraRight, TargetPitch - CurrentPitch));
    CameraRight = Normalized(Cross(LookDirection, GravityUp));

    Vector3 MoveForward =
        Normalized(Added(LookDirection, Scaled(GravityUp, -Dot(LookDirection, GravityUp))));
    Vector3 CurrentMovement = Added(Scaled(MoveForward, In.CurrentForward), Scaled(CameraRight, In.CurrentRight));
    float MovementLength = std::sqrt(Dot(CurrentMovement, CurrentMovement));
    if (MovementLength > 1.0f)
        CurrentMovement = Scaled(CurrentMovement, 1.0f / MovementLength);

    if (!AttachedToSurface) {
        if (MoveJumping(In, CurrentMovement)) {
            Reveal();
            return;
        }
    } else {
        float Step = MovementSpeed * In.Dt;
        // Split surface movement into steps no larger than 75% of the collision radius to prevent
        // wall tunneling. Resolve X before Z and stop immediately if either axis hits a barrier.
        int Substeps = std::max(1, (int)std::ceil(Step / (PlayerRadius * 0.75f)));
        float Substep = Step / Substeps;
        for (int Part = 0; Part < Substeps; ++Part) {
            if (MoveHorizontal(CurrentMovement.x, Substep, true) ||
                MoveHorizontal(CurrentMovement.z, Substep, false)) {
                Reveal();
                return;
            }
        }
    }

    Reveal();
    if (CurrentMazeData.Cell(Row(), Col()) == 'E')
        HasWon = true;
}

// Try one X- or Z-axis displacement after checking wall clearance, sweeping the accepted segment
// against hazards before committing it. Return true for lethal obstacle contact so the caller
// stops further movement immediately.
bool Game::MoveHorizontal(float CurrentMovement, float Step, bool AlongX) {
    const float NextX = AlongX ? Px + CurrentMovement * Step : Px;
    const float NextZ = AlongX ? Pz : Pz + CurrentMovement * Step;
    if (CanMove(NextX, NextZ)) {
        if (CheckObstacleCollision({Px, Py, Pz}, {NextX, Py, NextZ}, AttachedToSurface,
                                   AttachedToSurface))
            return true;
        Px = NextX;
        Pz = NextZ;
    }
    return false;
}

// Sweep the player's body from the previous pose to the proposed pose with each endpoint's
// surface orientation. On contact, set the terminal loss and ObstacleHit reason; return whether
// this movement caused failure.
bool Game::CheckObstacleCollision(Vector3 From, Vector3 To, bool FromCeiling, bool ToCeiling) {
    if (!HitsGravityObstacle(CurrentGravityObstacles, From, FromCeiling, To, ToCeiling))
        return false;
    HasLost = true;
    CurrentFailure = FailureReason::ObstacleHit;
    return true;
}

// Return a fraction from zero to one for the current quarter-second surface transfer. The clamped
// result is shared by displayed position and camera rotation interpolation.
float Game::TransitionBlend() const {
    return std::clamp(RollTime / 0.25f, 0.0f, 1.0f);
}

// Return the displayed eye position by interpolating from the captured transition pose to the
// logical player position. During an ordinary run this matches the current position; reading it
// does not advance simulation.
Vector3 Game::Position() const {
    float CurrentBlend = TransitionBlend();
    return Added(Scaled(TransitionFromPosition, 1.0f - CurrentBlend), Scaled({Px, Py, Pz}, CurrentBlend));
}
// Return the X component of the displayed position, including active surface-transition
// interpolation. Use col() when a logical maze column is needed instead of a rendered coordinate.
float Game::X() const {
    return Position().x;
}
// Return the displayed eye height in world units, including floor-relative jump height or a
// surface transition. This is a camera coordinate rather than the player's foot position.
float Game::Y() const {
    return Position().y;
}
// Return the Z component of the displayed position, including active surface-transition
// interpolation. Use row() when a logical maze row is needed instead of a rendered coordinate.
float Game::Z() const {
    return Position().z;
}
// Measure the logical look direction's horizontal angle in radians using atan2, from positive X
// toward positive Z. This read-only value is useful for map markers and movement-facing checks.
float Game::Yaw() const {
    return std::atan2(LookDirection.z, LookDirection.x);
}
// Normalize the interpolated look vector used by the renderer during a gravity transfer. The
// logical direction remains separate from the displayed transition so the camera can rotate
// smoothly.
Vector3 Game::CameraDirection() const {
    float CurrentBlend = TransitionBlend();
    return Normalized(
        Added(Scaled(TransitionFromDirection, 1.0f - CurrentBlend), Scaled(LookDirection, CurrentBlend)));
}
// Rotate the previous up vector through the current surface-transition angle and normalize it.
// The renderer uses this orientation to roll between floor and ceiling views.
Vector3 Game::CameraUp() const {
    return Normalized(RotateAround(RollFrom, RollAxis, RollAngle * TransitionBlend()));
}
// Report whether the logical player is attached to the ceiling. Side-wall collisions never set
// this flag; an active camera transition may still be interpolating toward that surface.
bool Game::SurfaceAttached() const {
    return AttachedToSurface;
}
// Return the game mode selected at construction without changing it. Input handling and rendering
// use this value to distinguish ordinary runs from Zero Gravity.
GameMode Game::Mode() const {
    return CurrentGameMode;
}
// Return the palette selected when this run was constructed. The value stays stable while
// playing, pausing, and showing results, so rendering cannot reroll the run's appearance.
MazeTheme Game::Theme() const {
    return CurrentMazeTheme;
}
// Return the stored maze-generation seed. Supply this seed, theme, size, and mode when
// constructing a run to reproduce its layout and visual selection for debugging.
std::uint32_t Game::Seed() const {
    return RunSeed;
}
// Convert the logical Z coordinate into a maze row with MazeGeometry::cellIndex. Collision and
// exit tests use this value independently of the camera's displayed transition position.
int Game::Row() const {
    return CellIndex(Pz);
}
// Convert the logical X coordinate into a maze column with MazeGeometry::cellIndex. Collision and
// exit tests use this value independently of the camera's displayed transition position.
int Game::Col() const {
    return CellIndex(Px);
}

// Read a cell's exploration state: 0 is unrevealed, 1 is revealed, and 2 is visited.
// Out-of-bounds coordinates return 0 instead of indexing outside the visibility vector.
int Game::SeenState(int CurrentR, int CurrentC) const {
    int N = CurrentMazeData.Size();
    if (CurrentR < 0 || CurrentR >= N || CurrentC < 0 || CurrentC >= N)
        return 0;
    return Seen[CurrentR * N + CurrentC];
}

// Return the remaining countdown in seconds; expiry clamps it to zero. This getter never advances
// time, so drawing a paused or completed run leaves the timer unchanged.
float Game::TimeLeft() const {
    return TimeLeftSec;
}
// Return the simulation seconds accumulated during active updates for this run. Pausing and
// terminal results stop accumulation; completed wins use this value for best-time comparisons.
float Game::TimeTaken() const {
    return ElapsedSec;
}
// Report whether movement reached the exit before the run failed. This reads the terminal result
// and does not recheck collision, update timers, or write a best-time record.
bool Game::Won() const {
    return HasWon;
}
// Report whether the run ended because its timer expired or an obstacle was hit. Call
// failureReason() when the end screen needs the specific cause.
bool Game::Lost() const {
    return HasLost;
}
// Return the stored loss cause so results can distinguish a timeout from lethal obstacle contact.
// FailureReason::None represents a run without a recorded failure.
FailureReason Game::GetFailureReason() const {
    return CurrentFailure;
}

// Borrow the run's immutable hazard layout for rendering and route checks. Normal mode leaves
// this vector empty; callers must not retain the reference after the Game is destroyed.
const std::vector<GravityObstacle>& Game::Obstacles() const {
    return CurrentGravityObstacles;
}

// Report whether either win or loss has become terminal. The application uses this to open
// results, while Game::update stops accepting further simulation changes.
bool Game::Finished() const {
    return HasWon || HasLost;
}
// Borrow the run's maze for rendering, generation preview, and route validation without copying
// its grid. The reference remains valid only while this Game exists.
const MazeData& Game::Maze() const {
    return CurrentMazeData;
}
