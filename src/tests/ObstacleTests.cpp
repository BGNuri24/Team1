#include "../logic/Game.h"
#include "../logic/GravityObstacles.h"
#include <cmath>
#include <stdexcept>

using namespace MazeGeometry;

// Throw a descriptive failure at the first unsafe obstacle layout or missed swept collision. The
// shared gameplay suite catches it and reports the failing barrier invariant.
static void requireObstacle(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Generate deterministic hazards across many mazes and verify dimensions, safe spawn and exit
// areas, spacing, and usable flip zones. Then test body sweeps against floor and ceiling
// barriers, including fast and vertical crossings.
void runObstacleTests() {
    for (int size : {10, 11, 15, 25, 40, 50}) {
        for (unsigned seed = 0; seed < 20; ++seed) {
            Game game(size, 1000, GameMode::ZeroGravity, seed);
            Game repeated(size, 1000, GameMode::ZeroGravity, seed);
            const auto& obstacles = game.obstacles();
            requireObstacle(obstacles.size() >= 2 && obstacles.size() == repeated.obstacles().size(),
                "gravity layouts must contain reproducible floor and ceiling barriers");
            for (std::size_t i = 0; i < obstacles.size(); ++i) {
                const auto& obstacle = obstacles[i];
                const auto& replay = repeated.obstacles()[i];
                requireObstacle(obstacle.row == replay.row && obstacle.col == replay.col &&
                    obstacle.acrossX == replay.acrossX && obstacle.ceiling == replay.ceiling,
                    "the same run seed must reproduce every barrier");
                requireObstacle(game.maze().cell(obstacle.row, obstacle.col) == '.' &&
                    obstacle.row + obstacle.col >= 4 &&
                    (size - 1 - obstacle.row) + (size - 1 - obstacle.col) >= 3,
                    "barriers must preserve safe spawn and exit approaches");
                const int r = obstacle.row, c = obstacle.col;
                requireObstacle(obstacle.acrossX
                    ? game.maze().cell(r - 1, c) != '#' && game.maze().cell(r + 1, c) != '#' &&
                        game.maze().cell(r, c - 1) == '#' && game.maze().cell(r, c + 1) == '#'
                    : game.maze().cell(r, c - 1) != '#' && game.maze().cell(r, c + 1) != '#' &&
                        game.maze().cell(r - 1, c) == '#' && game.maze().cell(r + 1, c) == '#',
                    "barriers must span straight corridors without obstructing a corner");
                for (std::size_t j = 0; j < i; ++j)
                    requireObstacle(std::abs(r - obstacles[j].row) + std::abs(c - obstacles[j].col) >= 4,
                        "barriers must leave room to flip between blocked surfaces");
            }
        }
    }
    Game normal(25, 1000, GameMode::Normal, 1234);
    requireObstacle(normal.obstacles().empty(), "normal mode must contain no gravity hazards");

    for (bool acrossX : {false, true}) {
        for (bool ceiling : {false, true}) {
            const std::vector<GravityObstacle> barrier = {{5, 5, acrossX, ceiling}};
            Vector3 before = {cellCenter(5), ceiling ? CeilingHeight - StandingEyeHeight : StandingEyeHeight,
                              cellCenter(5)};
            Vector3 after = before;
            (acrossX ? before.z : before.x) -= 0.8f;
            (acrossX ? after.z : after.x) += 0.8f;
            requireObstacle(hitsGravityObstacle(barrier, before, ceiling, after, ceiling),
                "a fast movement segment must not skip over the blocked surface");
            before.y = after.y = ceiling ? StandingEyeHeight : CeilingHeight - StandingEyeHeight;
            requireObstacle(!hitsGravityObstacle(barrier, before, !ceiling, after, !ceiling),
                "the opposite surface must remain clear across the entire barrier");
            Vector3 floor = {cellCenter(5), StandingEyeHeight, cellCenter(5)};
            Vector3 roof = {floor.x, CeilingHeight - StandingEyeHeight, floor.z};
            requireObstacle(hitsGravityObstacle(barrier, floor, false, roof, true) &&
                hitsGravityObstacle(barrier, roof, true, floor, false),
                "a gravity transfer through a barrier must be lethal in either direction");
        }
    }
}
