#include "../data/MazeData.h"
#include "../data/PlayerStore.h"
#include "../logic/Players.h"
#include "../logic/GameSettings.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <iostream>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
void runDatabaseTests();
void runUiTests();

// Fail the current test by throwing its explanatory message when an expected invariant is false.
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

// Verify maze dimensions, boundary handling, the exit, generation-step coverage and connectivity of
// every passage cell.
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
    pending.push({ 0, 0 });
    reached[0] = true;
    const int rowOffsets[] = { -1, 1, 0, 0 };
    const int colOffsets[] = { 0, 0, -1, 1 };

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
                pending.push({ nextRow, nextCol });
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

// Clear profile and best-time fixture files within this suite's isolated working directory before a
// persistence test.
static void clearTestData() {
    std::error_code directoryError;
    std::filesystem::create_directories("save_data", directoryError);
    require(!directoryError, "could not create test save-data directory");
    const char* files[] = {
        "save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
        "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/best_times.txt.tmp",
        "save_data/maze_game.db", "save_data/maze_game.db-journal"
    };
    for (const char* file : files) {
        std::error_code error;
        std::filesystem::remove(file, error);
        require(!error, std::string("could not clear test data: ") + file);
    }
}

// Check profile validation, capacity, record improvement, reload behavior and persisted removal of a
// player's records.
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

// Check active-profile selection, validation-result precedence, invalid run times and loading of
// duplicate or unknown-player records.
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

    // Load conflicting records to verify that the fastest time wins per maze size and unknown player
    // names are ignored.
    // Start a new database to exercise the one-time legacy import rather than an existing database.
    {
        std::ofstream legacyProfiles("save_data/players.txt");
        for (int i = 0; i < players.count(); ++i) legacyProfiles << players.name(i) << '\n';
    }
    std::filesystem::remove("save_data/maze_game.db");
    std::ofstream("save_data/best_times.txt") << "Bo 10 20\nBo 10 10\nBo 10 15\nBo 11 30\nmissing 10 1\n";
    PlayerStore reloaded;
    require(reloaded.bestTime("Bo", 10) == 10.0f && reloaded.bestTime("Bo", 11) == 30.0f,
        "record lookup must match both player and maze size");
    require(reloaded.bestTime("missing", 10) < 0, "unknown player record should be ignored");
}

// Verify that legacy Normal records and Zero Gravity records load, improve and disappear independently
// for the same player and size.
static void testSeparateModeRecords() {
    clearTestData();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/best_times.txt")
        << "Alex 25 40\nAlex 25 30 normal\n"
        << "Alex 25 12 zero_gravity\nAlex 25 15 zero_gravity\n"
        << "Alex 25 1 unknown\nAlex 25 2 normal extra\n";

    PlayerStore store;
    require(store.bestTime("Alex", 25) == 30.0f,
        "legacy and normal records must load into normal mode");
    require(store.bestTime("Alex", 25, GameMode::ZeroGravity) == 12.0f,
        "Zero Gravity duplicates must merge independently");
    require(store.recordBestTime("Alex", 25, 8.0f, GameMode::ZeroGravity),
        "Zero Gravity records should update");
    require(!store.recordBestTime("Alex", 25, 9.0f, GameMode::ZeroGravity),
        "slower Zero Gravity records should be rejected");
    require(store.bestTime("Alex", 25) == 30.0f,
        "Zero Gravity records must not replace normal records");
    require(store.recordBestTime("Alex", 25, 20.0f), "normal records should update independently");

    Players profiles;
    require(profiles.bestTime(25) == 20.0f && profiles.bestTime(25, GameMode::ZeroGravity) == 8.0f,
        "both modes must retain their records after reloading");
    require(profiles.recordWin(25, 5.0f, GameMode::ZeroGravity),
        "player management must record the chosen mode");
    PlayerStore reloaded;
    require(reloaded.bestTime("Alex", 25) == 20.0f &&
        reloaded.bestTime("Alex", 25, GameMode::ZeroGravity) == 5.0f,
        "recording a Zero Gravity win must preserve normal records on disk");
    require(profiles.remove(0) && profiles.add("Alex") == Players::ADDED,
        "players should be removable and reusable");
    PlayerStore removed;
    require(removed.bestTime("Alex", 25) < 0 &&
        removed.bestTime("Alex", 25, GameMode::ZeroGravity) < 0,
        "removing a player must clear records from both modes");
}

// Read a fixture file's binary contents so tests can compare saved bytes before and after an
// operation.
static std::string readFile(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Remove settings fixture files and blocker paths while allowing an ordinary file to occupy a tested
// directory path.
static void clearSettingsTestData() {
    std::error_code error;
    std::filesystem::create_directories("save_data", error);
    require(!error, "could not create settings test directory");
    // Remove only the settings files and blocker paths created inside this test suite's isolated working
    // directory.
    const char* paths[] = {"save_data/settings.txt.tmp/blocker.txt", "save_data/settings.txt/blocker.txt",
        "save_data/settings.txt", "save_data/settings.txt.tmp", "save_data/settings.txt.bak"};
    for (const char* path : paths) {
        error.clear();
        std::filesystem::remove(path, error);
        // Ignore not-a-directory errors when a blocker path's parent is an ordinary settings file rather
        // than a directory.
        if (error == std::errc::not_a_directory) error.clear();
        require(!error, std::string("could not clear settings test data: ")+path);
    }
}

// Exercise defaults, clamping, persistence, malformed content, backup recovery and failed-save retries
// without changing profile data.
static void testMouseSensitivitySettings() {
    clearSettingsTestData();
    const std::string playersBefore = readFile("save_data/players.txt");
    const std::string timesBefore = readFile("save_data/best_times.txt");
    GameSettings settings;
    require(settings.sensitivityPercent() == 100 && settings.sensitivityMultiplier() == 1.0f &&
        !settings.hasUnsavedChanges(), "missing settings should use clean default sensitivity");
    require(settings.save() && !std::filesystem::exists("save_data/settings.txt"),
        "loading or saving unchanged defaults must not create a settings file");
    settings.setSensitivityPercent(100);
    require(!settings.hasUnsavedChanges(), "setting the current sensitivity should remain clean");
    settings.setSensitivityPercent(125);
    require(settings.sensitivityMultiplier() == 1.25f && settings.hasUnsavedChanges(),
        "live sensitivity must update before saving");
    require(settings.save() && !settings.hasUnsavedChanges() &&
        GameSettings().sensitivityPercent() == 125, "sensitivity should persist across reloads");
    settings.setSensitivityPercent(-10);
    require(settings.sensitivityPercent() == GameSettings::MinSensitivity &&
        settings.sensitivityMultiplier() == .25f, "sensitivity should clamp at the lower limit");
    require(settings.save() && GameSettings().sensitivityPercent() == 25,
        "minimum sensitivity should round-trip");
    settings.setSensitivityPercent(999);
    require(settings.sensitivityPercent() == GameSettings::MaxSensitivity &&
        settings.sensitivityMultiplier() == 3.0f, "sensitivity should clamp at the upper limit");
    require(settings.save() && GameSettings().sensitivityPercent() == 300,
        "maximum sensitivity should round-trip");
    settings.resetSensitivity();
    require(settings.sensitivityPercent() == 100 && settings.hasUnsavedChanges() && settings.save() &&
        GameSettings().sensitivityPercent() == 100, "reset should restore and persist the default");

    const char* malformed[] = {"", "other_setting 100", "mouse_sensitivity", "mouse_sensitivity word",
        "mouse_sensitivity 24", "mouse_sensitivity 301", "mouse_sensitivity -25",
        "mouse_sensitivity 125 extra", "mouse_sensitivity 125\nmouse_sensitivity 200",
        "mouse_sensitivity 125.5", "mouse_sensitivity nan", "mouse_sensitivity 999999999999999999999999"};
    for (const char* content : malformed) {
        std::ofstream("save_data/settings.txt", std::ios::binary | std::ios::trunc) << content;
        GameSettings loaded;
        require(loaded.sensitivityPercent() == 100 && !loaded.hasUnsavedChanges(),
            "malformed or out-of-range settings should safely use defaults");
        require(readFile("save_data/settings.txt") == content,
            "loading malformed settings should not rewrite them");
    }
    std::ofstream("save_data/settings.txt", std::ios::binary | std::ios::trunc)
        << "\r\n mouse_sensitivity 175\r\n\t";
    require(GameSettings().sensitivityPercent() == 175, "settings should tolerate surrounding whitespace");

    // Simulate an interrupted replacement with only a backup present and verify that loading recovers
    // its value without writing files.
    std::filesystem::remove("save_data/settings.txt");
    std::ofstream("save_data/settings.txt.bak") << "mouse_sensitivity 150\n";
    GameSettings recovered;
    require(recovered.sensitivityPercent() == 150 && !std::filesystem::exists("save_data/settings.txt"),
        "an interrupted save should recover its backup without writing on load");
    recovered.setSensitivityPercent(175);
    require(recovered.save() && GameSettings().sensitivityPercent() == 175,
        "saving after backup recovery should retain the new value");

    // Block the temporary-file path to verify that save failure retains the new live value and preserves
    // the previous disk value.
    recovered.setSensitivityPercent(75);
    require(recovered.save(), "save-failure fixture should have a previous setting");
    std::filesystem::create_directory("save_data/settings.txt.tmp");
    std::ofstream("save_data/settings.txt.tmp/blocker.txt") << "keep";
    recovered.setSensitivityPercent(200);
    require(!recovered.save() && recovered.hasUnsavedChanges() && recovered.sensitivityPercent() == 200,
        "save failure must retain the desired live sensitivity for retry");
    require(GameSettings().sensitivityPercent() == 75,
        "save failure must not replace the previously saved setting");
    std::filesystem::remove("save_data/settings.txt.tmp/blocker.txt");
    std::filesystem::remove("save_data/settings.txt.tmp");
    require(recovered.save() && !recovered.hasUnsavedChanges() &&
        GameSettings().sensitivityPercent() == 200, "a failed save should succeed after the blocker is removed");

    std::filesystem::remove("save_data/settings.txt");
    std::filesystem::create_directory("save_data/settings.txt");
    std::ofstream("save_data/settings.txt/blocker.txt") << "keep";
    recovered.setSensitivityPercent(250);
    require(!recovered.save() && readFile("save_data/settings.txt/blocker.txt") == "keep",
        "a directory at the settings path must not be renamed or overwritten");
    clearSettingsTestData();
    require(readFile("save_data/players.txt") == playersBefore &&
        readFile("save_data/best_times.txt") == timesBefore,
        "settings changes must not modify profiles or best-time records");
}

// Run deterministic maze, profile and settings checks, returning a failing process status with the
// first assertion message.
int main() {
    try {
        std::srand(1234);
        testMaze(10);
        testMaze(11);
        testMaze(50);
        testPlayerStore();
        testProfilesAndValidation();
        testSeparateModeRecords();
        testMouseSensitivitySettings();
        runDatabaseTests();
        runUiTests();
    }
    catch (const std::exception& error) {
        std::cerr << "Test failure: " << error.what() << '\n';
        return 1;
    }
    std::cout << "All core tests passed.\n";
    return 0;
}
