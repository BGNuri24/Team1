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
void check(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Restore the parent test directory even when a database assertion throws.
struct Fixture {
    std::filesystem::path previous = std::filesystem::current_path();
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
    ~Fixture() { std::filesystem::current_path(previous); }
};

// Delete only this fixture's database, journal, legacy, and blocker files after operation-scoped
// connections have closed. Each storage scenario starts from a known state without removing
// unrelated project data.
void clear() {
    for (const char* path :
         {"save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
          "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/maze_game.db",
          "save_data/maze_game.db-journal", "src/assets/legacy/players.txt",
          "src/assets/legacy/best_times.txt", "assets/legacy/players.txt",
          "assets/legacy/best_times.txt"})
        std::filesystem::remove(path);
}

// Read exact binary contents from a fixture file for before-and-after comparisons. Migration
// tests use this to prove that importing legacy progress does not rewrite its source bytes.
std::string contents(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Import valid legacy rows once while preserving the fastest duplicate for each player, size, and
// mode. Delete or change imported records afterward and verify a later initialization does not
// resurrect stale text data.
void migration() {
    clear();
    std::ofstream("save_data/players.txt") << "Alex\nBo\n";
    std::ofstream("save_data/best_times.txt")
        << "Alex 25 40\nAlex 25 20 normal\nAlex 25 30\nAlex 25 12 zero_gravity\n"
           "Bo 10 9\nmissing 10 1\nAlex 9 1\nAlex 25 -1\nAlex 25 nan\n"
           "Alex 25 1 normal extra\nAlex 25 1 unsupported\n";
    const std::string original = contents("save_data/best_times.txt");
    PlayerStore store;
    check(store.storageStatus() == StorageStatus::Success,
          "SQLite initialization should import valid legacy rows");
    check((store.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 20 &&
              (store.bestTime("Alex", 25, GameMode::ZeroGravity).seconds.value_or(-1.0f)) == 12,
          "migration must merge duplicate times independently for each mode");
    check((store.bestTime("Bo", 10).seconds.value_or(-1.0f)) == 9 &&
              (store.bestTime("missing", 10).seconds.value_or(-1.0f)) < 0,
          "migration must preserve known players and ignore unknown names");
    check(contents("save_data/best_times.txt") == original,
          "migration must leave the legacy file unchanged");
    check(contents("save_data/maze_game.db").substr(0, 16) == std::string("SQLite format 3\0", 16),
          "scores must be stored in an actual SQLite database");
    std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
    PlayerStore reopened;
    check((reopened.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 20,
          "an existing database must not repeatedly import text records");
    check((reopened.remove(0) == StorageStatus::Success) &&
              (reopened.add("Alex") == StorageStatus::Success),
          "profile removal and reuse should succeed");
    PlayerStore reused;
    check(
        (reused.bestTime("Alex", 25).seconds.value_or(-1.0f)) < 0 &&
            (reused.bestTime("Bo", 10).seconds.value_or(-1.0f)) == 9,
        "deleted profiles must not recover obsolete legacy times or remove another player's rows");
}

// Use independent store instances to verify score comparisons see the latest committed value.
// Hold a database write lock to exercise explicit failure, then release it and verify subsequent
// operations recover.
void updatesAndLocks() {
    clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    PlayerStore first, second;
    check((first.recordBestTime("Alex", 25, 20) == RecordResult::Updated),
          "the first result must persist");
    check(second.recordBestTime("Alex", 25, 30) == RecordResult::Unchanged &&
              second.storageStatus() == StorageStatus::Success,
          "a stale store must reject a slower run without reporting a database failure");
    check((second.recordBestTime("Alex", 25, 10) == RecordResult::Updated) &&
              (first.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 10,
          "a store must read improvements made through another connection");
    sqlite3* lock = nullptr;
    check(sqlite3_open("save_data/maze_game.db", &lock) == SQLITE_OK, "lock fixture must open");
    check(sqlite3_exec(lock, "BEGIN EXCLUSIVE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "lock fixture must lock");
    const RecordResult writeResult = first.recordBestTime("Alex", 25, 5);
    const bool written = writeResult == RecordResult::Updated;
    const bool failed = writeResult == RecordResult::StorageError;
    sqlite3_exec(lock, "ROLLBACK", nullptr, nullptr, nullptr);
    sqlite3_close(lock);
    check(!written && failed && (first.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 10,
          "a failed locked write must report its failure and preserve the previous score");
    check((first.recordBestTime("Alex", 25, 5) == RecordResult::Updated) &&
              first.storageStatus() == StorageStatus::Success,
          "a write should succeed after the lock is released");
    check((second.remove(0) == StorageStatus::Success),
          "another store should be able to remove the profile");
    check(first.recordBestTime("Alex", 25, 1) == RecordResult::Invalid &&
              first.storageStatus() == StorageStatus::Success,
          "a stale profile list must not recreate scores for a deleted database profile");
}

// Force score deletion to fail after profile deletion has begun, then verify the transaction
// restores both tables. The cached profile list must also remain unchanged on this failed write.
void atomicProfileDeletion() {
    clear();
    std::ofstream("save_data/players.txt") << "Alex\nBo\n";
    const auto original = contents("save_data/players.txt");
    PlayerStore store;
    check((store.recordBestTime("Alex", 25, 12) == RecordResult::Updated) &&
              (store.recordBestTime("Bo", 25, 15) == RecordResult::Updated),
          "deletion fixture must save scores");
    sqlite3* database = nullptr;
    check(sqlite3_open("save_data/maze_game.db", &database) == SQLITE_OK,
          "trigger fixture must open");
    const int created = sqlite3_exec(database,
                                     "CREATE TRIGGER fail_score_delete BEFORE DELETE ON best_times "
                                     "BEGIN SELECT RAISE(ABORT,'test failure'); END;",
                                     nullptr, nullptr, nullptr);
    sqlite3_close(database);
    check(created == SQLITE_OK, "trigger fixture must install");
    check(!(store.remove(0) == StorageStatus::Success) && store.count() == 2 &&
              store.name(0) == "Alex",
          "a failed deletion must not change the in-memory profile list");
    PlayerStore restored;
    check(restored.count() == 2 && restored.name(0) == "Alex" &&
              (restored.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 12,
          "a failure after profile deletion must restore the persisted profile and all scores");
    check(sqlite3_open("save_data/maze_game.db", &database) == SQLITE_OK,
          "trigger removal fixture must open");
    const int dropped =
        sqlite3_exec(database, "DROP TRIGGER fail_score_delete", nullptr, nullptr, nullptr);
    sqlite3_close(database);
    check(dropped == SQLITE_OK && (store.remove(0) == StorageStatus::Success),
          "deletion must succeed after its blocker is removed");
    std::ofstream("save_data/players.txt") << "Ghost\n";
    PlayerStore reopened;
    check(reopened.count() == 1 && reopened.name(0) == "Bo" &&
              (reopened.bestTime("Bo", 25).seconds.value_or(-1.0f)) == 15,
          "SQLite profile order must remain authoritative after migration");
    check(contents("save_data/players.txt") != original,
          "fixture must have modified its legacy backup");
    check((reopened.add("Alex") == StorageStatus::Success) &&
              (reopened.bestTime("Alex", 25).seconds.value_or(-1.0f)) < 0,
          "reusing a deleted name must not resurrect its old scores");
}

// Start from an existing score-only schema and upgrade it without losing results or reimporting
// stale scores. Verify profile migration and later score updates use the retained database.
void scoreDatabaseUpgrade() {
    clear();
    std::ofstream("save_data/players.txt") << "Bo\nAlex\n";
    std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
    const auto originalProfiles = contents("save_data/players.txt");
    const auto originalScores = contents("save_data/best_times.txt");
    sqlite3* database = nullptr;
    check(sqlite3_open("save_data/maze_game.db", &database) == SQLITE_OK,
          "upgrade fixture must open");
    const int created =
        sqlite3_exec(database,
                     "CREATE TABLE best_times(player TEXT,maze_size INTEGER,mode TEXT,seconds "
                     "REAL,PRIMARY KEY(player,maze_size,mode));"
                     "INSERT INTO best_times VALUES('Alex',25,'normal',18);"
                     "CREATE TABLE storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
                     "INSERT INTO storage_metadata VALUES('legacy_best_times_imported','1');",
                     nullptr, nullptr, nullptr);
    sqlite3_close(database);
    check(created == SQLITE_OK, "upgrade fixture must create an older database schema");
    PlayerStore upgraded;
    check(upgraded.storageStatus() == StorageStatus::Success && upgraded.count() == 2 &&
              upgraded.name(0) == "Bo" &&
              (upgraded.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 18,
          "upgrade must preserve profile order and existing scores");
    check(contents("save_data/players.txt") == originalProfiles &&
              contents("save_data/best_times.txt") == originalScores,
          "database upgrade must not rewrite either legacy backup");
}

// Exercise a corrupt database and an incomplete legacy import, preserving the damaged source and
// preventing partial migration markers. Verify a repaired import can retry successfully on a
// later initialization.
void failuresAndRecovery() {
    clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/maze_game.db") << "not a database";
    const auto corrupt = contents("save_data/maze_game.db");
    PlayerStore broken;
    check(broken.storageStatus() != StorageStatus::Success &&
              broken.recordBestTime("Alex", 25, 10) == RecordResult::StorageError,
          "a corrupt database must not claim that a score was saved");
    check(contents("save_data/maze_game.db") == corrupt,
          "a corrupt database must not be overwritten");
    clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::filesystem::create_directory("save_data/best_times.txt");
    PlayerStore interrupted;
    check(interrupted.storageStatus() != StorageStatus::Success && interrupted.count() == 0,
          "an unreadable import source must not publish uncommitted profiles");
    std::filesystem::remove("save_data/best_times.txt");
    std::ofstream("save_data/best_times.txt") << "Alex 25 18\n";
    PlayerStore recovered;
    check(recovered.storageStatus() == StorageStatus::Success &&
              (recovered.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 18,
          "a failed migration must retry successfully without a committed partial marker");
    clear();
    std::ofstream("save_data/players.txt") << "Alex\n";
    std::ofstream("save_data/best_times.txt.bak") << "Alex 25 17\n";
    PlayerStore backup;
    check((backup.bestTime("Alex", 25).seconds.value_or(-1.0f)) == 17 &&
              !std::filesystem::exists("save_data/best_times.txt"),
          "migration should read a legacy backup without renaming it");
}

// Ignore old repository/package saves even when a previous build left them on disk. Real local
// profiles, scores and legacy backups must remain usable.
void bundledLegacyIgnored() {
    for (const char* directory : {"src/assets/legacy", "assets/legacy"}) {
        clear();
        const std::filesystem::path bundled(directory);
        std::filesystem::create_directories(bundled);
        const auto profiles = (bundled / "players.txt").string();
        const auto scores = (bundled / "best_times.txt").string();
        std::ofstream(profiles) << "Bundled\n";
        std::ofstream(scores) << "Bundled 15 21 normal\nLocal 15 2 normal\n";
        const auto originalProfiles = contents(profiles.c_str());
        const auto originalScores = contents(scores.c_str());

        GameDatabase fresh;
        const auto initialized = fresh.initialize();
        check(initialized.status == StorageStatus::Success && initialized.players.empty(),
              "fresh databases must ignore repository and packaged legacy profiles");
        const auto absent = fresh.bestTime("Bundled", 15, GameMode::Normal);
        check(absent.status == StorageStatus::Success && !absent.seconds,
              "fresh databases must ignore bundled scores");
        check(fresh.addPlayer("Local").status == StorageStatus::Success &&
                  fresh.record("Local", 15, 9, GameMode::Normal) == RecordResult::Updated,
              "fresh databases must still accept a player's own profile and result");
        GameDatabase reopened;
        const auto existing = reopened.initialize();
        check(existing.status == StorageStatus::Success && existing.players.size() == 1 &&
                  existing.players.front() == "Local" &&
                  reopened.bestTime("Local", 15, GameMode::Normal).seconds == 9,
              "existing profiles and scores must remain unchanged when bundled saves are present");

        std::filesystem::remove("save_data/maze_game.db");
        std::ofstream("save_data/players.txt") << "Local\n";
        GameDatabase local;
        const auto migrated = local.initialize();
        const auto missing = local.bestTime("Local", 15, GameMode::Normal);
        check(migrated.status == StorageStatus::Success && migrated.players.size() == 1 &&
                  migrated.players.front() == "Local" &&
                  missing.status == StorageStatus::Success && !missing.seconds,
              "a local legacy profile must not inherit a bundled score with the same name");
        check(contents(profiles.c_str()) == originalProfiles &&
                  contents(scores.c_str()) == originalScores,
              "ignored bundled saves must remain untouched");
    }

    clear();
    std::ofstream("save_data/players.txt.bak") << "Local\n";
    std::ofstream("save_data/best_times.txt.bak") << "Local 15 9\n";
    const auto backupProfiles = contents("save_data/players.txt.bak");
    const auto backupScores = contents("save_data/best_times.txt.bak");
    GameDatabase backup;
    const auto restored = backup.initialize();
    check(restored.status == StorageStatus::Success && restored.players.size() == 1 &&
              restored.players.front() == "Local" &&
              backup.bestTime("Local", 15, GameMode::Normal).seconds == 9 &&
              contents("save_data/players.txt.bak") == backupProfiles &&
              contents("save_data/best_times.txt.bak") == backupScores,
          "the player's own legacy backup profiles and scores must still migrate unchanged");
    clear();
}

// Distinguish absent scores, invalid submissions, unchanged records, and unavailable storage
// through explicit returned values. Later operations must not mutate an earlier result's meaning.
void typedResults() {
    std::filesystem::create_directories("isolated");
    std::filesystem::remove("isolated/test.db");
    GameDatabase database("isolated/test.db");
    check(database.initialize().status == StorageStatus::Success,
          "a custom database path must initialize independently");
    check(database.addPlayer("Alex").status == StorageStatus::Success,
          "typed profile insertion must succeed");
    check(database.addPlayer("Alex").status == StorageStatus::Duplicate,
          "duplicate insertion must have its own result");
    const auto missing = database.bestTime("Alex", 25, GameMode::Normal);
    check(missing.status == StorageStatus::Success && !missing.seconds,
          "a missing score must not masquerade as a storage error");
    check(database.record("Alex", 9, 10, GameMode::Normal) == RecordResult::Invalid,
          "invalid maze sizes must be identified");
    check(database.record("Alex", 25, 10, GameMode::Normal) == RecordResult::Updated,
          "a first score must report Updated");
    const auto unchanged = database.record("Alex", 25, 20, GameMode::Normal);
    const auto found = database.bestTime("Alex", 25, GameMode::Normal);
    check(unchanged == RecordResult::Unchanged && found.status == StorageStatus::Success &&
              found.seconds == 10,
          "later queries must not overwrite the outcome of an earlier operation");
    GameDatabase unopened("isolated/not-opened.db");
    const auto unavailable = unopened.bestTime("Alex", 25, GameMode::Normal);
    check(unavailable.status == StorageStatus::Unavailable && !unavailable.seconds &&
              unopened.record("Alex", 25, 10, GameMode::Normal) == RecordResult::StorageError,
          "uninitialized storage must report failure explicitly instead of a missing score");
}
}

// Run migration, concurrency, deletion, upgrade, validation, and recovery cases inside a separate
// fixture directory. Restore the caller's working directory and leave the player's real profiles
// and scores untouched.
void runDatabaseTests() {
    Fixture fixture;
    migration();
    updatesAndLocks();
    failuresAndRecovery();
    atomicProfileDeletion();
    scoreDatabaseUpgrade();
    typedResults();
    bundledLegacyIgnored();
}
