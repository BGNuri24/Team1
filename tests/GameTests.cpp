#include "../src/logic/Game.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void testTimerAndGravity() {
    Game timed(10, 1.0f);
    timed.update({0, 0, 0, 0, 1.25f, false});
    require(timed.lost() && timed.timeLeft() == 0 && !timed.won(), "timer should end the run");
    timed.update({1, 1, 50, 50, 1.0f, true});
    require(timed.x() == 0 && timed.z() == 0 && timed.timeTaken() == 1.25f,
            "finished run must stay frozen");

    Game gravity(10, 30.0f);
    gravity.update({0, 0, 0, -400, 0, false}); // Look up at the ceiling.
    gravity.update({0, 0, 0, 0, 0, true});
    require(gravity.surfaceAttached(), "G should attach to the aimed ceiling");
    gravity.update({0, 0, 0, 0, 0.25f, false});
    require(gravity.cameraUp().y < -0.99f && std::fabs(gravity.y() - 0.78f) < 0.001f,
            "ceiling transition should finish with inverted up direction");
    gravity.update({0, 0, 0, 0, 0, true});
    gravity.update({0, 0, 0, 0, 0.25f, false});
    require(!gravity.surfaceAttached() && gravity.cameraUp().y > 0.99f && gravity.y() == 0.5f,
            "G should return to the floor");
}

static void testReachableExit(int size) {
    Game game(size, 10000.0f);
    std::vector<int> previous(size * size, -1);
    std::queue<int> pending;
    pending.push(0);
    previous[0] = 0;
    const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
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
    require(previous.back() >= 0, "exit must be reachable");
    std::vector<int> path;
    for (int cell = size * size - 1; cell != 0; cell = previous[cell]) path.push_back(cell);
    std::reverse(path.begin(), path.end());
    constexpr float pi = 3.14159265358979323846f;
    for (int cell : path) {
        if (game.finished()) break;
        float dx = cell % size - game.x(), dz = cell / size - game.z();
        float turn = std::remainder(std::atan2(dz, dx) - game.yaw(), 2 * pi);
        game.update({0, 0, turn / 0.0025f, 0, 0, false});
        float distance = std::sqrt(dx * dx + dz * dz);
        for (int tick = 0; tick < 10 && !game.finished(); ++tick)
            game.update({1, 0, 0, 0, distance / 30.0f, false});
        require(!game.surfaceAttached(), "centered path movement should not attach to walls");
    }
    require(game.won() && !game.lost(), "following a valid path must win");
    require(game.score() == size * 100 + (int)std::ceil(game.timeLeft()) * 10,
            "win score formula must remain unchanged");
    float elapsed = game.timeTaken();
    game.update({1, 1, 100, 100, 1, true});
    require(game.timeTaken() == elapsed, "winning must freeze the timer");
}

int main() {
    try {
        std::srand(1234);
        testTimerAndGravity();
        testReachableExit(10);
        testReachableExit(11);
        testReachableExit(50);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Gameplay tests passed.\n";
}
