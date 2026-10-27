#pragma once
#include "raylib.h"
#include "../data/MazeData.h"
#include "../data/GameMode.h"
#include "../data/MazeGeometry.h"
#include <vector>

struct InputState {
    // -1..1
    float forward;
    // -1..1
    float right;
    float mouseDX, mouseDY;
    float dt;
    bool gravityToggle;
};

class Game {
public:
    Game(int mazeSize, float seconds, GameMode mode = GameMode::Normal);
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
    int row() const;
    int col() const;
    // 0 fog, 1 seen, 2 walked
    int seenState(int r, int c) const;
    float timeLeft() const;
    // seconds since the run started
    float timeTaken() const;
    int score() const;
    bool won() const;
    bool lost() const;
    bool finished() const;
    const MazeData& maze() const;
private:
    float transitionBlend() const;
    bool moveHorizontal(float movement, float step, bool alongX);
    bool isWall(float x, float z) const;
    bool canMove(float x, float z) const;
    bool hasSurfaceSupport(float x, float z) const;
    bool findAimedSurface(Vector3& normal, Vector3& contact) const;
    bool attachToSurface(const Vector3& normal, const Vector3& contact);
    bool transitionToAdjacentWall(int side);
    bool transitionToNearestWall(float x, float z);
    void beginSurfaceTransition(const Vector3& newUp, const Vector3& targetPosition,
        bool attached);
    void toggleGravity();
    void reveal();

    GameMode gameMode;
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
