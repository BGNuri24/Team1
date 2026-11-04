#include "../logic/Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

using namespace MazeGeometry;
void runJumpTests();

// Turn a failed gameplay expectation into an exception carrying the test's explanatory message.
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Ignore invalid frame durations so malformed timing input cannot move the run timer backward or make
// its state non-finite.
static void testInvalidFrameDuration() {
    Game game(10, 30.0f);
    game.update({ 1, 0, 0, 0, -1.0f, false });
    game.update({ 1, 0, 0, 0, std::numeric_limits<float>::quiet_NaN(), false });
    require(game.timeTaken() == 0.0f && game.timeLeft() == 30.0f && !game.finished(),
        "invalid frame durations must not change gameplay state");
}

// Check timer expiry and frozen results, then verify ceiling attachment and return-to-floor
// transitions in Zero Gravity.
static void testTimerAndGravity() {
    Game timed(10, 1.0f);
    timed.update({ 0, 0, 0, 0, 1.25f, false });
    require(timed.lost() && timed.timeLeft() == 0 && !timed.won(), "timer should end the run");
    timed.update({ 1, 1, 50, 50, 1.0f, true });
    require(timed.x() == 0 && timed.z() == 0 && timed.timeTaken() == 1.25f,
        "finished run must stay frozen");

    Game gravity(10, 30.0f, GameMode::ZeroGravity);
    require(gravity.mode() == GameMode::ZeroGravity, "gravity level must retain its mode");
    // Apply an upward look delta without advancing time so the following gravity toggle targets the
    // ceiling.
    gravity.update({ 0, 0, 0, -400, 0, false });
    gravity.update({ 0, 0, 0, 0, 0, true });
    require(gravity.surfaceAttached(), "G should attach to the aimed ceiling");
    gravity.update({ 0, 0, 0, 0, 0.25f, false });
    require(gravity.cameraUp().y < -0.99f && std::fabs(gravity.y() - (CeilingHeight - SurfaceOffset)) < 0.001f,
        "ceiling transition should finish with inverted up direction");
    gravity.update({ 0, 0, 0, 0, 0, true });
    gravity.update({ 0, 0, 0, 0, 0.25f, false });
    require(!gravity.surfaceAttached() && gravity.cameraUp().y > 0.99f && gravity.y() == StandingEyeHeight,
        "G should return to the floor");
}

// Confirm that Normal levels ignore gravity toggles and keep floor orientation when movement collides
// with a boundary.
static void testNormalGravity() {
    constexpr float pi = 3.14159265358979323846f;
    for (int size : {10, 15, 25, 40, 50}) {
        Game normal(size, 1000.0f);
        require(normal.mode() == GameMode::Normal, "new games should default to normal gravity");
        normal.update({ 0, 0, 0, -400, 0, false });
        normal.update({ 0, 0, 0, 0, 0.25f, true });
        require(!normal.surfaceAttached() && normal.cameraUp().y > 0.99f && normal.y() == StandingEyeHeight,
            "G must not attach to the ceiling in normal levels");
        normal.update({ 0, 0, 0, 400, 0, false });
        float turn = std::remainder(-pi / 2 - normal.yaw(), 2 * pi);
        normal.update({ 0, 0, turn / 0.0025f, 0, 0, false });
        for (int tick = 0; tick < 60; ++tick)
            normal.update({ 1, 0, 0, 0, 1.0f / 60, true });
        require(!normal.surfaceAttached() && normal.cameraUp().y > 0.99f && normal.y() == StandingEyeHeight,
            "walking into walls must not change gravity in normal levels");
        require(normal.z() >= -HalfCell + PlayerRadius - 0.001f && normal.row() == 0,
            "normal movement must still collide with the maze boundary");
    }
}

// Walk into a boundary in Zero Gravity and verify automatic attachment to the visible wall plane.
static void testZeroGravityWallAttachment() {
    Game game(10, 30.0f, GameMode::ZeroGravity);
    constexpr float pi = 3.14159265358979323846f;
    float turn = std::remainder(-pi / 2 - game.yaw(), 2 * pi);
    game.update({ 0, 0, turn / 0.0025f, 0, 0, false });
    for (int tick = 0; tick < 30 && !game.surfaceAttached(); ++tick)
        game.update({ 1, 0, 0, 0, 0.02f, false });
    require(game.surfaceAttached(), "Zero Gravity must retain wall attachment");
    game.update({ 0, 0, 0, 0, 0.25f, false });
    require(game.cameraUp().z > 0.99f, "Zero Gravity should orient the camera onto the wall");
    require(std::fabs(game.z() - (-HalfCell + SurfaceOffset)) < 0.001f,
        "automatic wall attachment must meet the visible widened wall plane");
}

// Check the widened corridor's usable space and ensure aimed wall attachment uses the same world-space
// dimensions.
static void testWideCorridorClearance() {
    Game game(15, 1000);
    constexpr float pi = 3.14159265358979323846f;
    float turn = std::remainder(-pi/2 - game.yaw(), 2*pi);
    game.update({0,0,turn/.0025f,0,0,false});
    game.update({1,0,0,0,.7f/MovementSpeed,false});
    require(game.z() < -.69f && game.row() == 0 && !game.surfaceAttached(),
        "the widened corridor must allow walking beyond the old half-unit wall");
    game.update({0,0,pi/.0025f,0,0,false});
    game.update({1,0,0,0,1.4f/MovementSpeed,false});
    require(game.z() > .69f && game.row() == 0,
        "the corridor must provide usable space across its full new width");
    require(game.y() == StandingEyeHeight, "widening must preserve normal standing height");

    Game aimed(15, 1000, GameMode::ZeroGravity);
    turn = std::remainder(-pi/2 - aimed.yaw(), 2*pi);
    aimed.update({0,0,turn/.0025f,0,0,false});
    aimed.update({0,0,0,0,0,true});
    aimed.update({0,0,0,0,.25f,false});
    require(aimed.surfaceAttached() && aimed.cameraUp().z > .99f &&
        std::fabs(aimed.z() - (-HalfCell+SurfaceOffset)) < .001f,
        "G ray aiming must hit the visible widened wall");
}

// Use breadth-first search to return a reachable passage path from the player's current cell to the
// target cell index.
static std::vector<int> pathTo(const Game& game, int target) {
    const int size = game.maze().size();
    const int start = game.row()*size+game.col();
    std::vector<int> previous(size * size, -1);
    std::queue<int> pending;
    pending.push(start);
    previous[start] = start;
    const int dr[] = { -1, 1, 0, 0 }, dc[] = { 0, 0, -1, 1 };
    while (!pending.empty()) {
        int cell = pending.front();
        pending.pop();
        for (int direction = 0; direction < 4; ++direction) {
            int row = cell / size + dr[direction], col = cell % size + dc[direction];
            if (game.maze().cell(row, col) == '#') continue;
            int next = row * size + col;
            if (previous[next] < 0) {
                previous[next] = cell;
                pending.push(next);
            }
        }
    }
    require(previous[target] >= 0, "destination must be reachable");
    std::vector<int> path;
    for (int cell = target; cell != start; cell = previous[cell]) path.push_back(cell);
    std::reverse(path.begin(), path.end());
    return path;
}

// Turn and move through cell centers using public game input, asserting that ordinary centered
// movement does not attach to walls.
static void followPath(Game& game, const std::vector<int>& path) {
    const int size = game.maze().size();
    constexpr float pi = 3.14159265358979323846f;
    for (int cell : path) {
        if (game.finished()) break;
        float dx = cellCenter(cell % size) - game.x(), dz = cellCenter(cell / size) - game.z();
        float turn = std::remainder(std::atan2(dz, dx) - game.yaw(), 2 * pi);
        game.update({ 0, 0, turn / 0.0025f, 0, 0, false });
        float distance = std::sqrt(dx * dx + dz * dz);
        for (int tick = 0; tick < 10 && !game.finished(); ++tick)
            game.update({ 1, 0, 0, 0, distance / (10.0f * MovementSpeed), false });
        require(!game.surfaceAttached(), "centered path movement should not attach to walls");
    }
}

// Reach a corridor with open space beyond a wall and confirm that a slow movement frame cannot cross
// the solid cell.
static void testNoWallTunneling() {
    Game game(25, 10000);
    const int size = game.maze().size();
    const int dr[] = {-1,1,0,0}, dc[] = {0,0,-1,1};
    int origin = -1, direction = -1;
    for (int row = 1; row < size-1 && origin < 0; ++row)
        for (int col = 1; col < size-1 && origin < 0; ++col) {
            if (game.maze().cell(row,col) == '#') continue;
            for (int d = 0; d < 4; ++d) {
                if (game.maze().cell(row+dr[d],col+dc[d]) == '#' &&
                    game.maze().cell(row+2*dr[d],col+2*dc[d]) == '.') {
                    origin = row*size+col;
                    direction = d;
                    break;
                }
            }
        }
    require(origin >= 0, "test maze needs a wall with an open corridor beyond it");
    followPath(game, pathTo(game,origin));
    require(game.row() == origin/size && game.col() == origin%size,
        "wall collision fixture should reach its starting corridor");
    constexpr float pi = 3.14159265358979323846f;
    float turn = std::remainder(std::atan2((float)dr[direction],(float)dc[direction])-game.yaw(),2*pi);
    game.update({0,0,turn/.0025f,0,0,false});
    game.update({1,0,0,0,2*CellSize/MovementSpeed,false});
    require(game.row() == origin/size && game.col() == origin%size && !game.surfaceAttached(),
        "a slow frame must not jump through an entire wall into another corridor");
}

// Follow a computed route to the exit and verify victory scoring plus frozen elapsed time after
// completion.
static void testReachableExit(int size, GameMode mode = GameMode::Normal) {
    Game game(size, 10000.0f, mode);
    followPath(game, pathTo(game,size*size-1));
    require(game.won() && !game.lost(), "following a valid path must win");
    require(game.score() == size * 100 + (int)std::ceil(game.timeLeft()) * 10,
        "win score formula must remain unchanged");
    float elapsed = game.timeTaken();
    game.update({ 1, 1, 100, 100, 1, true });
    require(game.timeTaken() == elapsed, "winning must freeze the timer");
}

// Run jump, gravity, clearance and route-completion checks across supported sizes with deterministic
// maze randomness.
int main() {
    try {
        std::srand(1234);
        runJumpTests();
        testTimerAndGravity();
        testInvalidFrameDuration();
        testNormalGravity();
        testZeroGravityWallAttachment();
        testWideCorridorClearance();
        testNoWallTunneling();
        for (int size : {10,11,15,25,40,50}) {
            testReachableExit(size);
            testReachableExit(size, GameMode::ZeroGravity);
        }
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Gameplay tests passed.\n";
}
