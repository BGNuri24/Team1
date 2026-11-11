#include "../data/PlayerStore.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <winsqlite/winsqlite3.h>

namespace {
// Throw a runtime_error with the supplied explanation when a storage expectation is false. This
// preserves database regression checks even when compiler settings disable ordinary assert.
void Check(bool Condition, const char* Message) {
    if (!Condition)
        throw std::runtime_error(Message);
}

// Restore the parent test directory even when a database assertion throws.
struct Fixture {
    std::filesystem::path Previous = std::filesystem::current_path();
    // Create and enter a separate database-cases fixture directory while retaining the caller's
    // previous working directory. Database helpers can then use normal relative save paths
    // without touching real player progress.
    Fixture() {
        std::filesystem::create_directories("database-cases/save_data");
        std::filesystem::current_path("database-cases");
    }
    // Restore the working directory captured by the fixture before leaving its scope. Scoped
    // database connections have already closed, so subsequent suites resume in their own expected
    // location.
    ~Fixture() { std::filesystem::current_path(Previous); }
};

// Delete only this fixture's database, journal, legacy, and blocker files after operation-scoped
// connections have closed. Each storage scenario starts from a known state without removing
// unrelated project data.
void Clear() {
    for (const char* Path :
         {"save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
          "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/maze_game.db",
          "save_data/maze_game.db-journal", "src/assets/legacy/players.txt",
          "src/assets/legacy/best_times.txt", "assets/legacy/players.txt",
          "assets/legacy/best_times.txt"})
        std::filesystem::remove(Path);
}

// Read exact binary contents from a fixture file for before-and-after comparisons. Migration
// tests use this to prove that importing legacy progress does not rewrite its source bytes.
std::string Contents(const char* Path) {
    std::ifstream CurrentInput(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(CurrentInput), std::istreambuf_iterator<char>()};
}

// Import valid legacy rows once while preserving the fastest duplicate for each player, size, and
// mode. Delete or change imported records afterward and verify a later initialization does not
// resurrect stale text data.
void Migration() {
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\nBo\n";
    std::ofstream("save_data/best_times.txt")
        << "Alex 25 40\nAlex 25 20 normal\nAlex 25 30\nAlex 25 12 zero_gravity\n"
           "Bo 10 9\nmissing 10 1\nAlex 9 1\nAlex 25 -1\nAlex 25 nan\n"
           "Alex 25 1 normal extra\nAlex 25 1 unsupported\n";
    const std::string Original = Contents("save_data/best_times.txt");
    PlayerStore CurrentStore;
    Check(CurrentStore.GetStorageStatus() == StorageStatus::Success,
          "SQLite initialization should import valid legacy rows");
    Check((CurrentStore.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 20 &&
              (CurrentStore.BestTime("Alex", 25, GameMode::ZeroGravity).Seconds.value_or(-1.0f)) == 12,
          "migration must merge duplicate times independently for each mode");
    Check((CurrentStore.BestTime("Bo", 10).Seconds.value_or(-1.0f)) == 9 &&
              (CurrentStore.BestTime("missing", 10).Seconds.value_or(-1.0f)) < 0,
          "migration must preserve known players and ignore unknown names");
    Check(Contents("save_data/best_times.txt") == Original,
          "migration must leave the legacy file unchanged");
    Check(Contents("save_data/maze_game.db").substr(0, 16) == std::string("SQLite format 3\0", 16),
          "scores must be stored in an actual SQLite database");
    std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
    PlayerStore Reopened;
    Check((Reopened.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 20,
          "an existing database must not repeatedly import text records");
    Check((Reopened.Remove(0) == StorageStatus::Success) &&
              (Reopened.Add("Alex") == StorageStatus::Success),
          "profile removal and reuse should succeed");
    PlayerStore Reused;
    Check(
        (Reused.BestTime("Alex", 25).Seconds.value_or(-1.0f)) < 0 &&
            (Reused.BestTime("Bo", 10).Seconds.value_or(-1.0f)) == 9,
        "deleted profiles must not recover obsolete legacy times or remove another player's rows");
}

// Use independent store instances to verify score comparisons see the latest committed value.
// Hold a database write lock to exercise explicit failure, then release it and verify subsequent
// operations recover.
void UpdatesAndLocks() {
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    PlayerStore First, Second;
    Check((First.RecordBestTime("Alex", 25, 20) == RecordResult::Updated),
          "the first result must persist");
    Check(Second.RecordBestTime("Alex", 25, 30) == RecordResult::Unchanged &&
              Second.GetStorageStatus() == StorageStatus::Success,
          "a stale store must reject a slower run without reporting a database failure");
    Check((Second.RecordBestTime("Alex", 25, 10) == RecordResult::Updated) &&
              (First.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 10,
          "a store must read improvements made through another connection");
    sqlite3* Lock = nullptr;
    Check(sqlite3_open("save_data/maze_game.db", &Lock) == SQLITE_OK, "lock fixture must open");
    Check(sqlite3_exec(Lock, "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "lock fixture must lock");
    const RecordResult WriteResult = First.RecordBestTime("Alex", 25, 5);
    const bool Written = WriteResult == RecordResult::Updated;
    const bool Failed = WriteResult == RecordResult::StorageError;
    sqlite3_exec(Lock, "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close(Lock);
    Check(!Written && Failed && (First.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 10,
          "a failed locked write must report its failure and preserve the previous score");
    Check((First.RecordBestTime("Alex", 25, 5) == RecordResult::Updated) &&
              First.GetStorageStatus() == StorageStatus::Success,
          "a write should succeed after the lock is released");
    Check((Second.Remove(0) == StorageStatus::Success),
          "another store should be able to remove the profile");
    Check(First.RecordBestTime("Alex", 25, 1) == RecordResult::Invalid &&
              First.GetStorageStatus() == StorageStatus::Success,
          "a stale profile list must not recreate scores for a deleted database profile");
}

// Force score deletion to fail after profile deletion has begun, then verify the transaction
// restores both tables. The cached profile list must also remain unchanged on this failed write.
void AtomicProfileDeletion() {
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\nBo\n";
    const auto Original = Contents("save_data/players.txt");
    PlayerStore CurrentStore;
    Check((CurrentStore.RecordBestTime("Alex", 25, 12) == RecordResult::Updated) &&
              (CurrentStore.RecordBestTime("Bo", 25, 15) == RecordResult::Updated),
          "deletion fixture must save scores");
    sqlite3* CurrentDatabase = nullptr;
    Check(sqlite3_open("save_data/maze_game.db", &CurrentDatabase) == SQLITE_OK,
          "trigger fixture must open");
    const int Created = sqlite3_exec(CurrentDatabase,
                                     "CREATE TRIGGER fail_score_delete BEFORE DELETE ON best_times "
                                     "BEGIN SELECT RAISE(ABORT,'test failure'); END;",
                                     nullptr, nullptr, nullptr);
    sqlite3_close(CurrentDatabase);
    Check(Created == SQLITE_OK, "trigger fixture must install");
    Check(!(CurrentStore.Remove(0) == StorageStatus::Success) && CurrentStore.Count() == 2 &&
              CurrentStore.Name(0) == "Alex",
          "a failed deletion must not change the in-memory profile list");
    PlayerStore Restored;
    Check(Restored.Count() == 2 && Restored.Name(0) == "Alex" &&
              (Restored.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 12,
          "a failure after profile deletion must restore the persisted profile and all scores");
    Check(sqlite3_open("save_data/maze_game.db", &CurrentDatabase) == SQLITE_OK,
          "trigger removal fixture must open");
    const int Dropped =
        sqlite3_exec(CurrentDatabase, "DROP TRIGGER fail_score_delete", nullptr, nullptr, nullptr);
    sqlite3_close(CurrentDatabase);
    Check(Dropped == SQLITE_OK && (CurrentStore.Remove(0) == StorageStatus::Success),
          "deletion must succeed after its blocker is removed");
    std::ofstream("save_data/players.txt") << "Ghost\n";
    PlayerStore Reopened;
    Check(Reopened.Count() == 1 && Reopened.Name(0) == "Bo" &&
              (Reopened.BestTime("Bo", 25).Seconds.value_or(-1.0f)) == 15,
          "SQLite profile order must remain authoritative after migration");
    Check(Contents("save_data/players.txt") != Original,
          "fixture must have modified its legacy backup");
    Check((Reopened.Add("Alex") == StorageStatus::Success) &&
              (Reopened.BestTime("Alex", 25).Seconds.value_or(-1.0f)) < 0,
          "reusing a deleted name must not resurrect its old scores");
}

// Start from an existing score-only schema and upgrade it without losing results or reimporting
// stale scores. Verify profile migration and later score updates use the retained database.
void ScoreDatabaseUpgrade() {
    Clear();
    std::ofstream("save_data/players.txt") << "Bo\nAlex\n";
    std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
    const auto OriginalProfiles = Contents("save_data/players.txt");
    const auto OriginalScores = Contents("save_data/best_times.txt");
    sqlite3* CurrentDatabase = nullptr;
    Check(sqlite3_open("save_data/maze_game.db", &CurrentDatabase) == SQLITE_OK,
          "upgrade fixture must open");
    const int Created =
        sqlite3_exec(CurrentDatabase,
                     "CREATE TABLE best_times(player TEXT,maze_size INTEGER,mode TEXT,seconds "
                     "REAL,PRIMARY KEY(player,maze_size,mode));"
                     "INSERT INTO best_times VALUES('Alex',25,'normal',18);"
                     "CREATE TABLE storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
                     "INSERT INTO storage_metadata VALUES('legacy_best_times_imported','1');",
                     nullptr, nullptr, nullptr);
    sqlite3_close(CurrentDatabase);
    Check(Created == SQLITE_OK, "upgrade fixture must create an older database schema");
    PlayerStore Upgraded;
    Check(Upgraded.GetStorageStatus() == StorageStatus::Success && Upgraded.Count() == 2 &&
              Upgraded.Name(0) == "Bo" &&
              (Upgraded.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 18,
          "upgrade must preserve profile order and existing scores");
    Check(Contents("save_data/players.txt") == OriginalProfiles &&
              Contents("save_data/best_times.txt") == OriginalScores,
          "database upgrade must not rewrite either legacy backup");
}

// Exercise a corrupt database and an incomplete legacy import, preserving the damaged source and
// preventing partial migration markers. Verify a repaired import can retry successfully on a
// later initialization.
void FailuresAndRecovery() {
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/maze_game.db") << "not a database";
    const auto Corrupt = Contents("save_data/maze_game.db");
    PlayerStore Broken;
    Check(Broken.GetStorageStatus() != StorageStatus::Success &&
              Broken.RecordBestTime("Alex", 25, 10) == RecordResult::StorageError,
          "a corrupt database must not claim that a score was saved");
    Check(Contents("save_data/maze_game.db") == Corrupt,
          "a corrupt database must not be overwritten");
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::filesystem::create_directory("save_data/best_times.txt");
    PlayerStore Interrupted;
    Check(Interrupted.GetStorageStatus() != StorageStatus::Success && Interrupted.Count() == 0,
          "an unreadable import source must not publish uncommitted profiles");
    std::filesystem::remove("save_data/best_times.txt");
    std::ofstream("save_data/best_times.txt") << "Alex 25 18\n";
    PlayerStore Recovered;
    Check(Recovered.GetStorageStatus() == StorageStatus::Success &&
              (Recovered.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 18,
          "a failed migration must retry successfully without a committed partial marker");
    Clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/best_times.txt.bak") << "Alex 25 17\n";
    PlayerStore Backup;
    Check((Backup.BestTime("Alex", 25).Seconds.value_or(-1.0f)) == 17 &&
              !std::filesystem::exists("save_data/best_times.txt"),
          "migration should read a legacy backup without renaming it");
}

// Ignore old repository/package saves even when a previous build left them on disk. Real local
// profiles, scores and legacy backups must remain usable.
void BundledLegacyIgnored() {
    for (const char* Directory : {"src/assets/legacy", "assets/legacy"}) {
        Clear();
        const std::filesystem::path CurrentBundled(Directory);
        std::filesystem::create_directories(CurrentBundled);
        const auto Profiles = (CurrentBundled / "players.txt").string();
        const auto Scores = (CurrentBundled / "best_times.txt").string();
        std::ofstream(Profiles) << "Bundled\n";
        std::ofstream(Scores) << "Bundled 15 21 normal\nLocal 15 2 normal\n";
        const auto OriginalProfiles = Contents(Profiles.c_str());
        const auto OriginalScores = Contents(Scores.c_str());

        GameDatabase Fresh;
        const auto Initialized = Fresh.Initialize();
        Check(Initialized.Status == StorageStatus::Success && Initialized.CurrentPlayers.empty(),
              "fresh databases must ignore repository and packaged legacy profiles");
        const auto Absent = Fresh.BestTime("Bundled", 15, GameMode::Normal);
        Check(Absent.Status == StorageStatus::Success && !Absent.Seconds,
              "fresh databases must ignore bundled scores");
        Check(Fresh.AddPlayer("Local").Status == StorageStatus::Success &&
                  Fresh.Record("Local", 15, 9, GameMode::Normal) == RecordResult::Updated,
              "fresh databases must still accept a player's own profile and result");
        GameDatabase Reopened;
        const auto CurrentExisting = Reopened.Initialize();
        Check(CurrentExisting.Status == StorageStatus::Success && CurrentExisting.CurrentPlayers.size() == 1 &&
                  CurrentExisting.CurrentPlayers.front() == "Local" &&
                  Reopened.BestTime("Local", 15, GameMode::Normal).Seconds == 9,
              "existing profiles and scores must remain unchanged when bundled saves are present");

        std::filesystem::remove("save_data/maze_game.db");
        std::ofstream("save_data/players.txt") << "Local\n";
        GameDatabase CurrentLocal;
        const auto Migrated = CurrentLocal.Initialize();
        const auto CurrentMissing = CurrentLocal.BestTime("Local", 15, GameMode::Normal);
        Check(Migrated.Status == StorageStatus::Success && Migrated.CurrentPlayers.size() == 1 &&
                  Migrated.CurrentPlayers.front() == "Local" &&
                  CurrentMissing.Status == StorageStatus::Success && !CurrentMissing.Seconds,
              "a local legacy profile must not inherit a bundled score with the same name");
        Check(Contents(Profiles.c_str()) == OriginalProfiles &&
                  Contents(Scores.c_str()) == OriginalScores,
              "ignored bundled saves must remain untouched");
    }

    Clear();
    std::ofstream("save_data/players.txt.bak") << "Local\n";
    std::ofstream("save_data/best_times.txt.bak") << "Local 15 9\n";
    const auto BackupProfiles = Contents("save_data/players.txt.bak");
    const auto BackupScores = Contents("save_data/best_times.txt.bak");
    GameDatabase Backup;
    const auto Restored = Backup.Initialize();
    Check(Restored.Status == StorageStatus::Success && Restored.CurrentPlayers.size() == 1 &&
              Restored.CurrentPlayers.front() == "Local" &&
              Backup.BestTime("Local", 15, GameMode::Normal).Seconds == 9 &&
              Contents("save_data/players.txt.bak") == BackupProfiles &&
              Contents("save_data/best_times.txt.bak") == BackupScores,
          "the player's own legacy backup profiles and scores must still migrate unchanged");
    Clear();
}

// Distinguish absent scores, invalid submissions, unchanged records, and unavailable storage
// through explicit returned values. Later operations must not mutate an earlier result's meaning.
void TypedResults() {
    std::filesystem::create_directories("isolated");
    std::filesystem::remove("isolated/test.db");
    GameDatabase CurrentDatabase("isolated/test.db");
    Check(CurrentDatabase.Initialize().Status == StorageStatus::Success,
          "a custom database path must initialize independently");
    Check(CurrentDatabase.AddPlayer("Alex").Status == StorageStatus::Success,
          "typed profile insertion must succeed");
    Check(CurrentDatabase.AddPlayer("Alex").Status == StorageStatus::Duplicate,
          "duplicate insertion must have its own result");
    const auto CurrentMissing = CurrentDatabase.BestTime("Alex", 25, GameMode::Normal);
    Check(CurrentMissing.Status == StorageStatus::Success && !CurrentMissing.Seconds,
          "a missing score must not masquerade as a storage error");
    Check(CurrentDatabase.Record("Alex", 9, 10, GameMode::Normal) == RecordResult::Invalid,
          "invalid maze sizes must be identified");
    Check(CurrentDatabase.Record("Alex", 25, 10, GameMode::Normal) == RecordResult::Updated,
          "a first score must report Updated");
    const auto CurrentUnchanged = CurrentDatabase.Record("Alex", 25, 20, GameMode::Normal);
    const auto Found = CurrentDatabase.BestTime("Alex", 25, GameMode::Normal);
    Check(CurrentUnchanged == RecordResult::Unchanged && Found.Status == StorageStatus::Success &&
              Found.Seconds == 10,
          "later queries must not overwrite the outcome of an earlier operation");
    GameDatabase Unopened("isolated/not-opened.db");
    const auto CurrentUnavailable = Unopened.BestTime("Alex", 25, GameMode::Normal);
    Check(CurrentUnavailable.Status == StorageStatus::Unavailable && !CurrentUnavailable.Seconds &&
              Unopened.Record("Alex", 25, 10, GameMode::Normal) == RecordResult::StorageError,
          "uninitialized storage must report failure explicitly instead of a missing score");
}
}

// Run migration, concurrency, deletion, upgrade, validation, and recovery cases inside a separate
// fixture directory. Restore the caller's working directory and leave the player's real profiles
// and scores untouched.
void RunDatabaseTests() {
    Fixture CurrentFixture;
    Migration();
    UpdatesAndLocks();
    FailuresAndRecovery();
    AtomicProfileDeletion();
    ScoreDatabaseUpgrade();
    TypedResults();
    BundledLegacyIgnored();
}
