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
// Run migration, concurrency, deletion, upgrade, validation, and recovery cases inside a separate
// fixture directory. Restore the caller's working directory and leave the player's real profiles
// and scores untouched.
void RunDatabaseTests();
// Create the hidden graphics context and run font-fitting, palette, resource, menu, and lighting
// checks. Export the small-window preview within the test output while keeping real gameplay and
// saves untouched.
void RunUiTests();
// Check every supported dimension, including even-size exit connectors, and verify deterministic
// layouts and previews. Also guard against difficulty regressing into long routes with too few
// decisions.
void RunMazeGenerationTests();

// Throw a descriptive runtime_error when an invariant is false so checks remain active in Debug
// and Release. The suite's main catches the exception and returns a failing process status.
static void Require(bool Condition, const std::string& Message) {
    if (!Condition) throw std::runtime_error(Message);
}

// Verify maze dimensions, boundary handling, the exit, generation-step coverage and connectivity
// of every passage cell.
static void TestMaze(int CurrentSize) {
    MazeData Maze(CurrentSize, 1234);
    Require(Maze.Size() == CurrentSize, "maze size does not match requested size");
    Require(Maze.Cell(0, 0) != '#', "maze start is blocked");
    Require(Maze.Cell(CurrentSize - 1, CurrentSize - 1) == 'E', "maze exit is missing");
    Require(Maze.Cell(-1, 0) == '#', "out-of-bounds cells should be walls");

    std::set<int> Steps;
    for (const MazeStep& Step : Maze.GenerationSteps()) {
        Require(Step.Row >= 0 && Step.Row < CurrentSize && Step.Col >= 0 && Step.Col < CurrentSize,
            "generation step is outside the maze");
        Steps.insert(Step.Row * CurrentSize + Step.Col);
    }

    std::vector<bool> Reached(CurrentSize * CurrentSize, false);
    std::queue<std::pair<int, int>> Pending;
    Pending.push({ 0, 0 });
    Reached[0] = true;
    const int RowOffsets[] = { -1, 1, 0, 0 };
    const int ColOffsets[] = { 0, 0, -1, 1 };

    while (!Pending.empty()) {
        const auto [row, col] = Pending.front();
        Pending.pop();
        for (int Direction = 0; Direction < 4; ++Direction) {
            int NextRow = row + RowOffsets[Direction];
            int NextCol = col + ColOffsets[Direction];
            if (Maze.Cell(NextRow, NextCol) == '#') continue;
            int Index = NextRow * CurrentSize + NextCol;
            if (!Reached[Index]) {
                Reached[Index] = true;
                Pending.push({ NextRow, NextCol });
            }
        }
    }

    for (int Row = 0; Row < CurrentSize; ++Row) {
        for (int Col = 0; Col < CurrentSize; ++Col) {
            if (Maze.Cell(Row, Col) != '#') {
                Require(Reached[Row * CurrentSize + Col], "maze contains an unreachable path cell");
                Require(Steps.count(Row * CurrentSize + Col) != 0,
                    "open maze cell is missing from generation steps");
            }
        }
    }
}

// Remove profile, score, and database fixture files inside this suite's isolated working
// directory. Resetting fixtures prevents a previous test's persisted state from affecting later
// assertions.
static void ClearTestData() {
    std::error_code DirectoryError;
    std::filesystem::create_directories("save_data", DirectoryError);
    Require(!DirectoryError, "could not create test save-data directory");
    const char* Files[] = {
        "save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
        "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/best_times.txt.tmp",
        "save_data/maze_game.db", "save_data/maze_game.db-journal"
    };
    for (const char* File : Files) {
        std::error_code Error;
        std::filesystem::remove(File, Error);
        Require(!Error, std::string("could not clear test data: ") + File);
    }
}

// Check profile validation, capacity, record improvement, reload behavior and persisted removal
// of a player's records.
static void TestPlayerStore() {
    ClearTestData();
    {
        PlayerStore CurrentPlayers;
        Require(CurrentPlayers.Count() == 0, "player store should start empty");
        Require(CurrentPlayers.Add("Alex1") == StorageStatus::Success, "valid player should be added");
        Require(CurrentPlayers.Add("Alex1") == StorageStatus::Duplicate, "duplicate player should be rejected");
        Require(CurrentPlayers.Add("bad-name") == StorageStatus::Invalid, "invalid player characters should be rejected");
        Require(CurrentPlayers.Add("") == StorageStatus::Invalid, "empty player name should be rejected");
        Require(CurrentPlayers.RecordBestTime("Alex1", 10, 41.5f) == RecordResult::Updated,
            "valid best time should be recorded");
        Require(CurrentPlayers.RecordBestTime("Alex1", 10, 45.0f) == RecordResult::Unchanged,
            "slower time should not replace the best time");
        Require(CurrentPlayers.RecordBestTime("Alex1", 10, 40.0f) == RecordResult::Updated,
            "faster time should replace the best time");
        Require(CurrentPlayers.RecordBestTime("missing", 10, 30.0f) == RecordResult::Invalid,
            "time for an unknown player should be rejected");
        Require(CurrentPlayers.RecordBestTime("Alex1", 9, 30.0f) == RecordResult::Invalid,
            "time for an invalid maze size should be rejected");

        Require(CurrentPlayers.Add("Bo") == StorageStatus::Success, "second player should be added");
        Require(CurrentPlayers.Add("Cy") == StorageStatus::Success, "third player should be added");
        Require(CurrentPlayers.Add("Dee") == StorageStatus::Success, "fourth player should be added");
        Require(CurrentPlayers.Add("Eve") == StorageStatus::Success, "fifth player should be added");
        Require(CurrentPlayers.Add("Finn") == StorageStatus::Full, "player limit should be enforced");
    }

    {
        PlayerStore CurrentPlayers;
        Require(CurrentPlayers.Count() == MaxPlayers, "players should persist after reload");
        Require(CurrentPlayers.Name(0) == "Alex1", "player order should persist after reload");
        Require(CurrentPlayers.BestTime("Alex1", 10).Seconds == 40.0f,
            "best time should persist after reload");
        Require(CurrentPlayers.Remove(0) == StorageStatus::Success, "existing player should be removable");
        Require(CurrentPlayers.Remove(100) == StorageStatus::Invalid, "invalid player index should be rejected");
    }

    PlayerStore CurrentPlayers;
    Require(CurrentPlayers.Count() == MaxPlayers - 1, "removal should persist");
    Require(!CurrentPlayers.BestTime("Alex1", 10).Seconds,
        "removing a player should remove their best times");
}

// Check active-profile selection, validation-result precedence, invalid run times and loading of
// duplicate or unknown-player records.
static void TestProfilesAndValidation() {
    ClearTestData();
    Players CurrentPlayers;
    Require(CurrentPlayers.ActiveName() == "Guest", "empty profile list should use Guest");
    Require(CurrentPlayers.Add("bad-name") == Players::INVALID, "profile must reject punctuation");
    Require(CurrentPlayers.Add("1234567890123") == Players::INVALID, "profile name limit changed");
    Require(CurrentPlayers.Add("Alex_1234567") == Players::ADDED, "12-character name should work");
    Require(CurrentPlayers.Add("Alex_1234567") == Players::DUPLICATE, "duplicate result changed");
    Require(CurrentPlayers.Add("Bo") == Players::ADDED, "second profile should work");
    CurrentPlayers.SetActive(1);
    Require(CurrentPlayers.RecordWin(10, 12.5f) == RecordResult::Updated, "active player should save their time");
    Require(CurrentPlayers.RecordWin(51, 1.0f) == RecordResult::Invalid, "oversized maze record must be rejected");
    Require(CurrentPlayers.RecordWin(10, std::numeric_limits<float>::infinity()) == RecordResult::Invalid,
        "infinite time must be rejected");
    Require(CurrentPlayers.RecordWin(10, std::numeric_limits<float>::quiet_NaN()) == RecordResult::Invalid,
        "NaN time must be rejected");
    Require(CurrentPlayers.Remove(0), "earlier profile should be removable");
    Require(CurrentPlayers.ActiveName() == "Bo" && CurrentPlayers.BestTime(10).Seconds == 12.5f,
        "removing an earlier profile must preserve active player and record");
    Require(CurrentPlayers.Add("Cy") == Players::ADDED && CurrentPlayers.Add("Dee") == Players::ADDED &&
        CurrentPlayers.Add("Eve") == Players::ADDED && CurrentPlayers.Add("Finn") == Players::ADDED,
        "remaining profile slots should work");
    Require(CurrentPlayers.Add("Bo") == Players::FULL, "full must take priority over duplicate");
    Require(CurrentPlayers.Add("") == Players::INVALID, "invalid must take priority over full");

    // Load conflicting records to verify that the fastest time wins per maze size and unknown player
    // names are ignored.
    // Start a new database to exercise the one-time legacy import rather than an existing database.
    {
        std::ofstream LegacyProfiles("save_data/players.txt");
        for (int I = 0; I < CurrentPlayers.Count(); ++I) LegacyProfiles << CurrentPlayers.Name(I) << '\n';
    }
    std::filesystem::remove("save_data/maze_game.db");
    std::ofstream("save_data/best_times.txt") << "Bo 10 20\nBo 10 10\nBo 10 15\nBo 11 30\nmissing 10 1\n";
    PlayerStore Reloaded;
    Require(Reloaded.BestTime("Bo", 10).Seconds == 10.0f && Reloaded.BestTime("Bo", 11).Seconds == 30.0f,
        "record lookup must match both player and maze size");
    Require(!Reloaded.BestTime("missing", 10).Seconds, "unknown player record should be ignored");
}

// Verify that legacy Normal records and Zero Gravity records load, improve and disappear
// independently for the same player and size.
static void TestSeparateModeRecords() {
    ClearTestData();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/best_times.txt")
        << "Alex 25 40\nAlex 25 30 normal\n"
        << "Alex 25 12 zero_gravity\nAlex 25 15 zero_gravity\n"
        << "Alex 25 1 unknown\nAlex 25 2 normal extra\n";

    PlayerStore CurrentStore;
    Require(CurrentStore.BestTime("Alex", 25).Seconds == 30.0f,
        "legacy and normal records must load into normal mode");
    Require(CurrentStore.BestTime("Alex", 25, GameMode::ZeroGravity).Seconds == 12.0f,
        "Zero Gravity duplicates must merge independently");
    Require(CurrentStore.RecordBestTime("Alex", 25, 8.0f, GameMode::ZeroGravity) == RecordResult::Updated,
        "Zero Gravity records should update");
    Require(CurrentStore.RecordBestTime("Alex", 25, 9.0f, GameMode::ZeroGravity) == RecordResult::Unchanged,
        "slower Zero Gravity records should be rejected");
    Require(CurrentStore.BestTime("Alex", 25).Seconds == 30.0f,
        "Zero Gravity records must not replace normal records");
    Require(CurrentStore.RecordBestTime("Alex", 25, 20.0f) == RecordResult::Updated, "normal records should update independently");

    Players Profiles;
    Require(Profiles.BestTime(25).Seconds == 20.0f && Profiles.BestTime(25, GameMode::ZeroGravity).Seconds == 8.0f,
        "both modes must retain their records after reloading");
    Require(Profiles.RecordWin(25, 5.0f, GameMode::ZeroGravity) == RecordResult::Updated,
        "player management must record the chosen mode");
    PlayerStore Reloaded;
    Require(Reloaded.BestTime("Alex", 25).Seconds == 20.0f &&
        Reloaded.BestTime("Alex", 25, GameMode::ZeroGravity).Seconds == 5.0f,
        "recording a Zero Gravity win must preserve normal records on disk");
    Require(Profiles.Remove(0) && Profiles.Add("Alex") == Players::ADDED,
        "players should be removable and reusable");
    PlayerStore Removed;
    Require(!Removed.BestTime("Alex", 25).Seconds &&
        !Removed.BestTime("Alex", 25, GameMode::ZeroGravity).Seconds,
        "removing a player must clear records from both modes");
}

// Read a fixture file as exact binary bytes so tests can compare contents before and after
// storage operations. The caller chooses an isolated fixture path rather than the player's real
// save files.
static std::string ReadFile(const char* Path) {
    std::ifstream CurrentInput(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(CurrentInput), std::istreambuf_iterator<char>()};
}

// Remove settings fixtures, backups, temporary files, and intentional blocker paths in the
// isolated test directory. Restore a clean starting point before exercising malformed input and
// failed-save recovery.
static void ClearSettingsTestData() {
    std::error_code Error;
    std::filesystem::create_directories("save_data", Error);
    Require(!Error, "could not create settings test directory");
    // Remove only the settings files and blocker paths created inside this test suite's isolated working
    // directory.
    const char* Paths[] = {"save_data/settings.txt.tmp/blocker.txt", "save_data/settings.txt/blocker.txt",
        "save_data/settings.txt", "save_data/settings.txt.tmp", "save_data/settings.txt.bak"};
    for (const char* Path : Paths) {
        Error.clear();
        std::filesystem::remove(Path, Error);
        // Ignore not-a-directory errors when a blocker path's parent is an ordinary settings file rather
        // than a directory.
        if (Error == std::errc::not_a_directory) Error.clear();
        Require(!Error, std::string("could not clear settings test data: ")+Path);
    }
}

// Exercise defaults, clamping, persistence, malformed content, backup recovery and failed-save
// retries without changing profile data.
static void TestMouseSensitivitySettings() {
    ClearSettingsTestData();
    const std::string PlayersBefore = ReadFile("save_data/players.txt");
    const std::string TimesBefore = ReadFile("save_data/best_times.txt");
    GameSettings CurrentSettings;
    Require(CurrentSettings.SensitivityPercent() == 100 && CurrentSettings.SensitivityMultiplier() == 1.0f &&
        !CurrentSettings.HasUnsavedChanges(), "missing settings should use clean default sensitivity");
    Require(CurrentSettings.Save() && !std::filesystem::exists("save_data/settings.txt"),
        "loading or saving unchanged defaults must not create a settings file");
    CurrentSettings.SetSensitivityPercent(100);
    Require(!CurrentSettings.HasUnsavedChanges(), "setting the current sensitivity should remain clean");
    CurrentSettings.SetSensitivityPercent(125);
    Require(CurrentSettings.SensitivityMultiplier() == 1.25f && CurrentSettings.HasUnsavedChanges(),
        "live sensitivity must update before saving");
    Require(CurrentSettings.Save() && !CurrentSettings.HasUnsavedChanges() &&
        GameSettings().SensitivityPercent() == 125, "sensitivity should persist across reloads");
    CurrentSettings.SetSensitivityPercent(-10);
    Require(CurrentSettings.SensitivityPercent() == GameSettings::MinSensitivity &&
        CurrentSettings.SensitivityMultiplier() == .25f, "sensitivity should clamp at the lower limit");
    Require(CurrentSettings.Save() && GameSettings().SensitivityPercent() == 25,
        "minimum sensitivity should round-trip");
    CurrentSettings.SetSensitivityPercent(999);
    Require(CurrentSettings.SensitivityPercent() == GameSettings::MaxSensitivity &&
        CurrentSettings.SensitivityMultiplier() == 3.0f, "sensitivity should clamp at the upper limit");
    Require(CurrentSettings.Save() && GameSettings().SensitivityPercent() == 300,
        "maximum sensitivity should round-trip");
    CurrentSettings.ResetSensitivity();
    Require(CurrentSettings.SensitivityPercent() == 100 && CurrentSettings.HasUnsavedChanges() && CurrentSettings.Save() &&
        GameSettings().SensitivityPercent() == 100, "reset should restore and persist the default");

    const char* Malformed[] = {"", "other_setting 100", "mouse_sensitivity", "mouse_sensitivity word",
        "mouse_sensitivity 24", "mouse_sensitivity 301", "mouse_sensitivity -25",
        "mouse_sensitivity 125 extra", "mouse_sensitivity 125\nmouse_sensitivity 200",
        "mouse_sensitivity 125.5", "mouse_sensitivity nan", "mouse_sensitivity 999999999999999999999999"};
    for (const char* Content : Malformed) {
        std::ofstream("save_data/settings.txt", std::ios::binary | std::ios::trunc) << Content;
        GameSettings Loaded;
        Require(Loaded.SensitivityPercent() == 100 && !Loaded.HasUnsavedChanges(),
            "malformed or out-of-range settings should safely use defaults");
        Require(ReadFile("save_data/settings.txt") == Content,
            "loading malformed settings should not rewrite them");
    }
    std::ofstream("save_data/settings.txt", std::ios::binary | std::ios::trunc)
        << "\r\n mouse_sensitivity 175\r\n\t";
    Require(GameSettings().SensitivityPercent() == 175, "settings should tolerate surrounding whitespace");

    // Simulate an interrupted replacement with only a backup present and verify that loading recovers
    // its value without writing files.
    std::filesystem::remove("save_data/settings.txt");
    std::ofstream("save_data/settings.txt.bak") << "mouse_sensitivity 150\n";
    GameSettings Recovered;
    Require(Recovered.SensitivityPercent() == 150 && !std::filesystem::exists("save_data/settings.txt"),
        "an interrupted save should recover its backup without writing on load");
    Recovered.SetSensitivityPercent(175);
    Require(Recovered.Save() && GameSettings().SensitivityPercent() == 175,
        "saving after backup recovery should retain the new value");

    // Block the temporary-file path to verify that save failure retains the new live value and preserves
    // the previous disk value.
    Recovered.SetSensitivityPercent(75);
    Require(Recovered.Save(), "save-failure fixture should have a previous setting");
    std::filesystem::create_directory("save_data/settings.txt.tmp");
    std::ofstream("save_data/settings.txt.tmp/blocker.txt") << "keep";
    Recovered.SetSensitivityPercent(200);
    Require(!Recovered.Save() && Recovered.HasUnsavedChanges() && Recovered.SensitivityPercent() == 200,
        "save failure must retain the desired live sensitivity for retry");
    Require(GameSettings().SensitivityPercent() == 75,
        "save failure must not replace the previously saved setting");
    std::filesystem::remove("save_data/settings.txt.tmp/blocker.txt");
    std::filesystem::remove("save_data/settings.txt.tmp");
    Require(Recovered.Save() && !Recovered.HasUnsavedChanges() &&
        GameSettings().SensitivityPercent() == 200, "a failed save should succeed after the blocker is removed");

    std::filesystem::remove("save_data/settings.txt");
    std::filesystem::create_directory("save_data/settings.txt");
    std::ofstream("save_data/settings.txt/blocker.txt") << "keep";
    Recovered.SetSensitivityPercent(250);
    Require(!Recovered.Save() && ReadFile("save_data/settings.txt/blocker.txt") == "keep",
        "a directory at the settings path must not be renamed or overwritten");
    ClearSettingsTestData();
    Require(ReadFile("save_data/players.txt") == PlayersBefore &&
        ReadFile("save_data/best_times.txt") == TimesBefore,
        "settings changes must not modify profiles or best-time records");
}

// Run the core maze, profile, settings, database, and UI checks, reporting the first thrown
// failure. Return a nonzero process exit code when a check fails so RunTests.ps1 can stop the
// overall validation.
int main() {
    try {
        TestMaze(10);
        TestMaze(11);
        TestMaze(50);
        RunMazeGenerationTests();
        TestPlayerStore();
        TestProfilesAndValidation();
        TestSeparateModeRecords();
        TestMouseSensitivitySettings();
        RunDatabaseTests();
        RunUiTests();
    }
    catch (const std::exception& Error) {
        std::cerr << "Test failure: " << Error.what() << '\n';
        return 1;
    }
    std::cout << "All core tests passed.\n";
    return 0;
}
