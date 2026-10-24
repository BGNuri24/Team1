#include "../src/data/MazeData.h"
#include "../src/data/PlayerStore.h"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static void testMaze(int size) {
    MazeData maze(size);
    require(maze.size() == size, "maze size does not match requested size");
    require(maze.cell(0, 0) != '#', "maze start is blocked");
    require(maze.cell(size - 1, size - 1) == 'E', "maze exit is missing");
    require(maze.cell(-1, 0) == '#', "out-of-bounds cells should be walls");

    std::set<int> steps;
    for (const MazeStep& step : maze.generationSteps()) {
        require(step.row >= 0 && step.row < size && step.col >= 0 && step.col < size,
                "generation step is outside the maze");
        steps.insert(step.row * size + step.col);
    }

    std::vector<bool> reached(size * size, false);
    std::queue<std::pair<int, int>> pending;
    pending.push({0, 0});
    reached[0] = true;
    const int rowOffsets[] = {-1, 1, 0, 0};
    const int colOffsets[] = {0, 0, -1, 1};

    while (!pending.empty()) {
        const auto [row, col] = pending.front();
        pending.pop();
        for (int direction = 0; direction < 4; ++direction) {
            int nextRow = row + rowOffsets[direction];
            int nextCol = col + colOffsets[direction];
            if (maze.cell(nextRow, nextCol) == '#') continue;
            int index = nextRow * size + nextCol;
            if (!reached[index]) {
                reached[index] = true;
                pending.push({nextRow, nextCol});
            }
        }
    }

    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (maze.cell(row, col) != '#') {
                require(reached[row * size + col], "maze contains an unreachable path cell");
                require(steps.count(row * size + col) != 0,
                        "open maze cell is missing from generation steps");
            }
        }
    }
}

static void clearTestData() {
    const char* files[] = {
        "players.txt", "players.txt.bak", "players.txt.tmp",
        "best_times.txt", "best_times.txt.bak", "best_times.txt.tmp"
    };
    for (const char* file : files) {
        std::error_code error;
        std::filesystem::remove(file, error);
        require(!error, std::string("could not clear test data: ") + file);
    }
}

static void testPlayerStore() {
    clearTestData();
    {
        PlayerStore players;
        require(players.count() == 0, "player store should start empty");
        require(players.add("Alex1"), "valid player should be added");
        require(!players.add("Alex1"), "duplicate player should be rejected");
        require(!players.add("bad-name"), "invalid player characters should be rejected");
        require(!players.add(""), "empty player name should be rejected");
        require(players.recordBestTime("Alex1", 10, 41.5f),
                "valid best time should be recorded");
        require(!players.recordBestTime("Alex1", 10, 45.0f),
                "slower time should not replace the best time");
        require(players.recordBestTime("Alex1", 10, 40.0f),
                "faster time should replace the best time");
        require(!players.recordBestTime("missing", 10, 30.0f),
                "time for an unknown player should be rejected");
        require(!players.recordBestTime("Alex1", 9, 30.0f),
                "time for an invalid maze size should be rejected");

        require(players.add("Bo"), "second player should be added");
        require(players.add("Cy"), "third player should be added");
        require(players.add("Dee"), "fourth player should be added");
        require(players.add("Eve"), "fifth player should be added");
        require(!players.add("Finn"), "player limit should be enforced");
    }

    {
        PlayerStore players;
        require(players.count() == MAX_PLAYERS, "players should persist after reload");
        require(players.name(0) == "Alex1", "player order should persist after reload");
        require(players.bestTime("Alex1", 10) == 40.0f,
                "best time should persist after reload");
        require(players.remove(0), "existing player should be removable");
        require(!players.remove(100), "invalid player index should be rejected");
    }

    PlayerStore players;
    require(players.count() == MAX_PLAYERS - 1, "removal should persist");
    require(players.bestTime("Alex1", 10) < 0.0f,
            "removing a player should remove their best times");
}

int main() {
    try {
        std::srand(1234);
        testMaze(10);
        testMaze(11);
        testMaze(50);
        testPlayerStore();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
    std::cout << "All core tests passed.\n";
    return 0;
}
