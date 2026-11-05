#pragma once
#include "../data/GameMode.h"
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
};

class Game {
public:
    Game(int mazeSize, float seconds, GameMode mode = GameMode::Normal,
         std::optional<std::uint32_t> seed = std::nullopt,
         std::optional<MazeTheme> theme = std::nullopt);
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void update(const InputState& in);

    float x() const;
    float y() const;
    float z() const;
    Vector3 position() const;
    float yaw() const;
    Vector3 cameraDirection() const;
    Vector3 cameraUp() const;
    bool surfaceAttached() const;
    GameMode mode() const;
    // Return the palette selected once when this run was created.
    MazeTheme theme() const;
    // Reuse this value and theme() with the same level parameters to reproduce a loaded run.
    std::uint32_t seed() const;
    int row() const;
    int col() const;
    // Return the exploration state: 0 is unrevealed, 1 is visible but unvisited, and 2 is visited.
    int seenState(int r, int c) const;
    float timeLeft() const;
    // Elapsed simulation time in seconds; it advances during an active run and stops when the run
    // finishes.
    float timeTaken() const;
    int score() const;
    bool won() const;
    bool lost() const;
    bool finished() const;
    const MazeData& maze() const;

private:
    bool moveJumping(const InputState& input, const Vector3& wish);
    JumpMovement jumpMovement;
    float transitionBlend() const;
    bool moveHorizontal(float movement, float step, bool alongX);
    bool isWall(float x, float z) const;
    bool canMove(float x, float z) const;
    bool hasSurfaceSupport(float x, float z) const;
    bool findAimedSurface(Vector3& normal, Vector3& contact) const;
    bool attachToSurface(const Vector3& normal, const Vector3& contact);
    bool transitionToAdjacentWall(int side);
    bool transitionToNearestWall(float x, float z);
    void beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition, bool attached);
    void toggleGravity();
    void reveal();

    GameMode gameMode;
    std::uint32_t runSeed;
    MazeTheme mazeTheme;
    MazeData mazeData;
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
    int points;
    std::vector<char> seen;
};
