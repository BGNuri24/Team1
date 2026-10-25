#include "../src/data/MazeData.h"
#include "../src/data/PlayerStore.h"
#include "../src/logic/Players.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
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
    std::error_code directoryError;
    std::filesystem::create_directories("save_data", directoryError);
    require(!directoryError, "could not create test save-data directory");
    const char* files[] = {
        "save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
        "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/best_times.txt.tmp"
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

static void testProfilesAndValidation() {
    clearTestData();
    Players players;
    require(players.activeName() == "Guest", "empty profile list should use Guest");
    require(players.add("bad-name") == Players::INVALID, "profile must reject punctuation");
    require(players.add("1234567890123") == Players::INVALID, "profile name limit changed");
    require(players.add("Alex_1234567") == Players::ADDED, "12-character name should work");
    require(players.add("Alex_1234567") == Players::DUPLICATE, "duplicate result changed");
    require(players.add("Bo") == Players::ADDED, "second profile should work");
    players.setActive(1);
    require(players.recordWin(10, 12.5f), "active player should save their time");
    require(!players.recordWin(51, 1.0f), "oversized maze record must be rejected");
    require(!players.recordWin(10, std::numeric_limits<float>::infinity()),
            "infinite time must be rejected");
    require(!players.recordWin(10, std::numeric_limits<float>::quiet_NaN()),
            "NaN time must be rejected");
    require(players.remove(0), "earlier profile should be removable");
    require(players.activeName() == "Bo" && players.bestTime(10) == 12.5f,
            "removing an earlier profile must preserve active player and record");
    require(players.add("Cy") == Players::ADDED && players.add("Dee") == Players::ADDED &&
            players.add("Eve") == Players::ADDED && players.add("Finn") == Players::ADDED,
            "remaining profile slots should work");
    require(players.add("Bo") == Players::FULL, "full must take priority over duplicate");
    require(players.add("") == Players::INVALID, "invalid must take priority over full");

    // Loading must retain the fastest duplicate record and distinguish maze sizes.
    std::ofstream("save_data/best_times.txt") << "Bo 10 20\nBo 10 10\nBo 10 15\nBo 11 30\nmissing 10 1\n";
    PlayerStore reloaded;
    require(reloaded.bestTime("Bo", 10) == 10.0f && reloaded.bestTime("Bo", 11) == 30.0f,
            "record lookup must match both player and maze size");
    require(reloaded.bestTime("missing", 10) < 0, "unknown player record should be ignored");
}

int main() {
    try {
        std::srand(1234);
        testMaze(10);
        testMaze(11);
        testMaze(50);
        testPlayerStore();
        testProfilesAndValidation();
    } catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
    std::cout << "All core tests passed.\n";
    return 0;
}
