#include "../logic/Game.h"
#include "../logic/MazeThemeSelection.h"
#include <array>
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
void runObstacleTests();

// Turn a failed gameplay expectation into an exception carrying the test's explanatory message.
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Ignore invalid frame durations so malformed timing input cannot move the run timer backward or make
// its state non-finite.
static void testInvalidFrameDuration() {
    Game game(10, 30.0f, GameMode::Normal, 1234);
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
    require(timed.lost() && timed.failureReason() == FailureReason::TimeExpired && timed.timeLeft() == 0 && !timed.won(), "timer should end the run");
    timed.update({ 1, 1, 50, 50, 1.0f, true });
    require(timed.x() == 0 && timed.z() == 0 && timed.timeTaken() == 1.25f,
        "finished run must stay frozen");

    Game gravity(10, 30.0f, GameMode::ZeroGravity);
    require(gravity.mode() == GameMode::ZeroGravity, "gravity level must retain its mode");
    // Apply an upward look delta without advancing time so the following gravity toggle targets the
    // ceiling.
    gravity.update({ 0, 0, 0, -400, 0, false });
    gravity.update({ 0, 0, 0, 0, 0, true });
    require(gravity.surfaceAttached(), "G should switch directly to the ceiling");
    gravity.update({ 0, 0, 0, 0, 0.25f, false });
    require(gravity.cameraUp().y < -0.99f && std::fabs(gravity.y() - (CeilingHeight - StandingEyeHeight)) < 0.001f,
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

// Side-wall contact must remain ordinary collision on both surfaces, even when G is pressed
// while facing that wall. A gravity flip preserves corridor heading and horizontal position.
static void testZeroGravityWallCollision() {
    constexpr float pi = 3.14159265358979323846f;
    for (bool ceiling : {false, true}) {
        for (float target : {-pi / 2, pi}) {
            Game game(10, 30.0f, GameMode::ZeroGravity, 1234);
            if (ceiling) {
                game.update({0, 0, 0, 0, 0, true});
                game.update({0, 0, 0, 0, .25f, false});
            }
            float turn = std::remainder(target - game.yaw(), 2 * pi);
            game.update({0, 0, turn / (.0025f * (ceiling ? -1 : 1)), 0, 0, false});
            for (int tick = 0; tick < 60; ++tick)
                game.update({1, 0, 0, 0, .02f, false});
            require(game.surfaceAttached() == ceiling && std::fabs(game.cameraUp().y) > .99f,
                "left and right wall collisions must never change gravity");
            const Vector3 before = game.position();
            const float heading = game.yaw();
            game.update({0, 0, 0, 0, 0, true});
            game.update({0, 0, 0, 0, .25f, false});
            require(game.surfaceAttached() != ceiling && game.x() == before.x && game.z() == before.z,
                "G must flip vertically even when aiming directly at a side wall");
            require(std::fabs(std::remainder(game.yaw() - heading, 2 * pi)) < .001f,
                "flipping must preserve forward corridor heading");
        }
    }
}

// Check that ordinary walking still uses the full widened corridor.
static void testWideCorridorClearance() {
    Game game(15, 1000, GameMode::Normal, 1234);
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
static void followPath(Game& game, const std::vector<int>& path, int collisionTarget = -1) {
    const int size = game.maze().size();
    constexpr float pi = 3.14159265358979323846f;
    for (int cell : path) {
        if (game.finished()) break;
        for (const GravityObstacle& obstacle : game.obstacles()) {
            if (obstacle.row * size + obstacle.col != cell) continue;
            const bool ceiling = cell == collisionTarget ? obstacle.ceiling : !obstacle.ceiling;
            if (game.surfaceAttached() != ceiling) {
                game.update({0, 0, 0, 0, 0, true});
                game.update({0, 0, 0, 0, .25f, false});
            }
        }
        float dx = cellCenter(cell % size) - game.x(), dz = cellCenter(cell / size) - game.z();
        float turn = std::remainder(std::atan2(dz, dx) - game.yaw(), 2 * pi);
        game.update({ 0, 0, turn / (0.0025f * game.cameraUp().y), 0, 0, false });
        float distance = std::sqrt(dx * dx + dz * dz);
        const int ticks = cell == collisionTarget ? 1 : 10;
        for (int tick = 0; tick < ticks && !game.finished(); ++tick)
            game.update({ 1, 0, 0, 0, distance / (ticks * MovementSpeed), false });
        require(std::fabs(game.cameraUp().y) > .99f, "path movement must stay on floor or ceiling");
    }
}

// Reach a corridor with open space beyond a wall and confirm that a slow movement frame cannot cross
// the solid cell.
static void testNoWallTunneling() {
    Game game(25, 10000, GameMode::Normal, 1234);
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
static void testReachableExit(int size, GameMode mode = GameMode::Normal, unsigned seed = 1234) {
    Game game(size, 10000.0f, mode, seed);
    followPath(game, pathTo(game,size*size-1));
    require(game.won() && !game.lost(), "following a valid path must win");
    require(game.score() == size * 100 + (int)std::ceil(game.timeLeft()) * 10,
        "win score formula must remain unchanged");
    float elapsed = game.timeTaken();
    game.update({ 1, 1, 100, 100, 1, true });
    require(game.timeTaken() == elapsed, "winning must freeze the timer");
}

// Deliberately cross each barrier type on its blocked surface and verify immediate failure,
// a frozen result, and no victory points or timer-expiry explanation.
static void testObstacleFailure() {
    for (bool ceiling : {false, true}) {
        Game game(25, 10000, GameMode::ZeroGravity, 1234);
        auto target = std::find_if(game.obstacles().begin(), game.obstacles().end(),
            [&](const GravityObstacle& obstacle) { return obstacle.ceiling == ceiling; });
        require(target != game.obstacles().end(), "fixture needs both barrier types");
        const int cell = target->row * game.maze().size() + target->col;
        followPath(game, pathTo(game, cell), cell);
        require(game.lost() && !game.won() && game.failureReason() == FailureReason::ObstacleHit &&
            game.timeLeft() > 0 && game.score() == 0, "touching either barrier type must fail the run");
        const Vector3 stopped = game.position();
        const float time = game.timeTaken();
        game.update({1, 1, 30, 20, 1, true, true});
        require(game.timeTaken() == time && game.x() == stopped.x && game.y() == stopped.y &&
            game.z() == stopped.z, "a failed run must freeze all movement, gravity and timing");
    }
}

// Passing below a ceiling barrier is safe, but jumping or flipping into that same barrier must
// fail immediately rather than allowing the camera transfer to bypass collision.
static void testVerticalObstacleCollision() {
    for (bool jump : {false, true}) {
        Game game(25, 10000, GameMode::ZeroGravity, 1234);
        auto target = std::find_if(game.obstacles().begin(), game.obstacles().end(),
            [](const GravityObstacle& obstacle) { return obstacle.ceiling; });
        require(target != game.obstacles().end(), "fixture needs a ceiling barrier");
        followPath(game, pathTo(game, target->row * game.maze().size() + target->col));
        require(!game.finished() && !game.surfaceAttached(), "the floor below a ceiling barrier is safe");
        game.update({0, 0, 0, 0, jump ? .15f : 0.0f, !jump, jump});
        require(game.lost() && game.failureReason() == FailureReason::ObstacleHit,
            "jumping or flipping into a ceiling barrier must fail");
    }
}

// Run jump, gravity, clearance and route-completion checks across supported sizes with deterministic
// maze randomness.
// Cover every non-repeating random-draw mapping and ensure visual randomness leaves maze carving unchanged.
static void testRunThemes() {
    for (int previous = -1; previous < MazeThemeCount; ++previous) {
        std::array<bool, MazeThemeCount> found{};
        const int choices = previous < 0 ? MazeThemeCount : MazeThemeCount - 1;
        for (int draw = 0; draw < choices; ++draw) {
            const int selected = (int)themeFromDraw(draw, previous);
            require(selected >= 0 && selected < MazeThemeCount && selected != previous &&
                        !found[selected],
                    "random draws must map uniquely to valid themes excluding the previous one");
            found[selected] = true;
        }
    }
    MazeData baseline(15, 9917);
    Game themed(15, 100, GameMode::Normal, 9917);
    Game reproduced(15, 100, GameMode::Normal, themed.seed());
    require(themed.theme() == reproduced.theme(), "the same seed must reproduce its default theme");
    for (int row = 0; row < 15; ++row)
        for (int col = 0; col < 15; ++col)
            require(themed.maze().cell(row, col) == baseline.cell(row, col) &&
                        reproduced.maze().cell(row, col) == baseline.cell(row, col),
                    "choosing a visual theme must not change the maze-carving random sequence");
    const MazeTheme chosen = themed.theme();
    for (int frame = 0; frame < 20; ++frame)
        themed.update({0, 0, 0, 0, 1.0f / 60, false});
    require(themed.theme() == chosen, "a run's palette must stay fixed during gameplay updates");
    MazeThemeSelector selector(1234), repeated(1234);
    MazeTheme previous = selector.next();
    require(repeated.next() == previous, "seeded theme sequences must be reproducible");
    for (int run = 0; run < 12; ++run) {
        Game next(10, 100, run % 2 ? GameMode::Normal : GameMode::ZeroGravity, run,
                  selector.next());
        require(next.theme() == repeated.next(),
                "an independent seeded selector must reproduce every choice");
        require(next.theme() != previous && (int)next.theme() >= 0 &&
                    (int)next.theme() < MazeThemeCount,
                "new runs must choose an allowed theme without immediately repeating one");
        previous = next.theme();
    }
}


int main() {
    try {
        testRunThemes();
        runJumpTests();
        runObstacleTests();
        testObstacleFailure();
        testVerticalObstacleCollision();
        testTimerAndGravity();
        testInvalidFrameDuration();
        testNormalGravity();
        testZeroGravityWallCollision();
        testWideCorridorClearance();
        testNoWallTunneling();
        for (int size : {10,11,15,25,40,50}) {
            testReachableExit(size);
            for (unsigned seed : {0u, 1u, 1234u, 9917u})
                testReachableExit(size, GameMode::ZeroGravity, seed);
        }
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Gameplay tests passed.\n";
}
