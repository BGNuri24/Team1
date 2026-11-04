#include "../data/PlayerStore.h"
#include <winsqlite/winsqlite3.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
    // Fail the storage suite with a descriptive message instead of relying on disabled release asserts.
    void check(bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    }

    // Restore the parent test directory even when a database assertion throws.
    struct Fixture {
        std::filesystem::path previous = std::filesystem::current_path();
        Fixture() {
            std::filesystem::create_directories("database-cases/save_data");
            std::filesystem::current_path("database-cases");
        }
        ~Fixture() { std::filesystem::current_path(previous); }
    };

    // Remove only fixture-owned files; all database connections are scoped to individual operations.
    void clear() {
        for (const char* path : {"save_data/players.txt", "save_data/players.txt.bak", "save_data/players.txt.tmp",
            "save_data/best_times.txt", "save_data/best_times.txt.bak", "save_data/maze_game.db",
            "save_data/maze_game.db-journal"}) std::filesystem::remove(path);
    }

    // Read exact bytes so migration can prove it never rewrites a user's legacy score file.
    std::string contents(const char* path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    // Import valid rows once, retain the fastest duplicate per key, and never resurrect deleted scores.
    void migration() {
        clear();
        std::ofstream("save_data/players.txt") << "Alex\nBo\n";
        std::ofstream("save_data/best_times.txt") <<
            "Alex 25 40\nAlex 25 20 normal\nAlex 25 30\nAlex 25 12 zero_gravity\n"
            "Bo 10 9\nmissing 10 1\nAlex 9 1\nAlex 25 -1\nAlex 25 nan\n"
            "Alex 25 1 normal extra\nAlex 25 1 unsupported\n";
        const std::string original = contents("save_data/best_times.txt");
        PlayerStore store;
        check(!store.bestTimeStorageFailed(), "SQLite initialization should import valid legacy rows");
        check(store.bestTime("Alex",25) == 20 && store.bestTime("Alex",25,GameMode::ZeroGravity) == 12,
            "migration must merge duplicate times independently for each mode");
        check(store.bestTime("Bo",10) == 9 && store.bestTime("missing",10) < 0,
            "migration must preserve known players and ignore unknown names");
        check(contents("save_data/best_times.txt") == original, "migration must leave the legacy file unchanged");
        check(contents("save_data/maze_game.db").substr(0,16) == std::string("SQLite format 3\0",16),
            "scores must be stored in an actual SQLite database");
        std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
        PlayerStore reopened;
        check(reopened.bestTime("Alex",25) == 20, "an existing database must not repeatedly import text records");
        check(reopened.remove(0) && reopened.add("Alex"), "profile removal and reuse should succeed");
        PlayerStore reused;
        check(reused.bestTime("Alex",25) < 0 && reused.bestTime("Bo",10) == 9,
            "deleted profiles must not recover obsolete legacy times or remove another player's rows");
    }

    // Verify independent connections always compare against the latest stored result and recover after a lock.
    void updatesAndLocks() {
        clear();
        std::ofstream("save_data/players.txt") << "Alex\n";
        PlayerStore first, second;
        check(first.recordBestTime("Alex",25,20), "the first result must persist");
        check(!second.recordBestTime("Alex",25,30) && !second.bestTimeStorageFailed(),
            "a stale store must reject a slower run without reporting a database failure");
        check(second.recordBestTime("Alex",25,10) && first.bestTime("Alex",25) == 10,
            "a store must read improvements made through another connection");
        sqlite3* lock = nullptr;
        check(sqlite3_open("save_data/maze_game.db",&lock) == SQLITE_OK, "lock fixture must open");
        check(sqlite3_exec(lock,"BEGIN EXCLUSIVE",nullptr,nullptr,nullptr) == SQLITE_OK, "lock fixture must lock");
        const bool written = first.recordBestTime("Alex",25,5);
        const bool failed = first.bestTimeStorageFailed();
        sqlite3_exec(lock,"ROLLBACK",nullptr,nullptr,nullptr);
        sqlite3_close(lock);
        check(!written && failed && first.bestTime("Alex",25) == 10,
            "a failed locked write must report its failure and preserve the previous score");
        check(first.recordBestTime("Alex",25,5) && !first.bestTimeStorageFailed(),
            "a write should succeed after the lock is released");
        check(second.remove(0), "another store should be able to remove the profile");
        check(!first.recordBestTime("Alex",25,1) && !first.bestTimeStorageFailed(),
            "a stale profile list must not recreate scores for a deleted database profile");
    }

    // Force the score deletion to fail after profile deletion and verify that the transaction restores both.
    void atomicProfileDeletion() {
        clear();
        std::ofstream("save_data/players.txt") << "Alex\nBo\n";
        const auto original = contents("save_data/players.txt");
        PlayerStore store;
        check(store.recordBestTime("Alex",25,12) && store.recordBestTime("Bo",25,15), "deletion fixture must save scores");
        sqlite3* database = nullptr;
        check(sqlite3_open("save_data/maze_game.db",&database) == SQLITE_OK, "trigger fixture must open");
        const int created = sqlite3_exec(database,
            "CREATE TRIGGER fail_score_delete BEFORE DELETE ON best_times BEGIN SELECT RAISE(ABORT,'test failure'); END;",
            nullptr,nullptr,nullptr);
        sqlite3_close(database);
        check(created == SQLITE_OK, "trigger fixture must install");
        check(!store.remove(0) && store.count() == 2 && store.name(0) == "Alex",
            "a failed deletion must not change the in-memory profile list");
        PlayerStore restored;
        check(restored.count() == 2 && restored.name(0) == "Alex" && restored.bestTime("Alex",25) == 12,
            "a failure after profile deletion must restore the persisted profile and all scores");
        check(sqlite3_open("save_data/maze_game.db",&database) == SQLITE_OK, "trigger removal fixture must open");
        const int dropped = sqlite3_exec(database,"DROP TRIGGER fail_score_delete",nullptr,nullptr,nullptr);
        sqlite3_close(database);
        check(dropped == SQLITE_OK && store.remove(0), "deletion must succeed after its blocker is removed");
        std::ofstream("save_data/players.txt") << "Ghost\n";
        PlayerStore reopened;
        check(reopened.count() == 1 && reopened.name(0) == "Bo" && reopened.bestTime("Bo",25) == 15,
            "SQLite profile order must remain authoritative after migration");
        check(contents("save_data/players.txt") != original, "fixture must have modified its legacy backup");
        check(reopened.add("Alex") && reopened.bestTime("Alex",25) < 0,
            "reusing a deleted name must not resurrect its old scores");
    }

    // Upgrade an existing scores-only database without replacing its results or reimporting stale scores.
    void scoreDatabaseUpgrade() {
        clear();
        std::ofstream("save_data/players.txt") << "Bo\nAlex\n";
        std::ofstream("save_data/best_times.txt") << "Alex 25 1\n";
        const auto originalProfiles = contents("save_data/players.txt");
        const auto originalScores = contents("save_data/best_times.txt");
        sqlite3* database = nullptr;
        check(sqlite3_open("save_data/maze_game.db",&database) == SQLITE_OK, "upgrade fixture must open");
        const int created = sqlite3_exec(database,
            "CREATE TABLE best_times(player TEXT,maze_size INTEGER,mode TEXT,seconds REAL,PRIMARY KEY(player,maze_size,mode));"
            "INSERT INTO best_times VALUES('Alex',25,'normal',18);"
            "CREATE TABLE storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
            "INSERT INTO storage_metadata VALUES('legacy_best_times_imported','1');",nullptr,nullptr,nullptr);
        sqlite3_close(database);
        check(created == SQLITE_OK, "upgrade fixture must create an older database schema");
        PlayerStore upgraded;
        check(!upgraded.bestTimeStorageFailed() && upgraded.count() == 2 && upgraded.name(0) == "Bo" &&
            upgraded.bestTime("Alex",25) == 18, "upgrade must preserve profile order and existing scores");
        check(contents("save_data/players.txt") == originalProfiles && contents("save_data/best_times.txt") == originalScores,
            "database upgrade must not rewrite either legacy backup");
    }

    // Preserve corrupt databases and allow an incomplete import to retry without a partial import marker.
    void failuresAndRecovery() {
        clear();
        std::ofstream("save_data/players.txt") << "Alex\n";
        std::ofstream("save_data/maze_game.db") << "not a database";
        const auto corrupt = contents("save_data/maze_game.db");
        PlayerStore broken;
        check(broken.bestTimeStorageFailed() && !broken.recordBestTime("Alex",25,10),
            "a corrupt database must not claim that a score was saved");
        check(contents("save_data/maze_game.db") == corrupt, "a corrupt database must not be overwritten");
        clear();
        std::ofstream("save_data/players.txt") << "Alex\n";
        std::filesystem::create_directory("save_data/best_times.txt");
        PlayerStore interrupted;
        check(interrupted.bestTimeStorageFailed() && interrupted.count() == 0,
            "an unreadable import source must not publish uncommitted profiles");
        std::filesystem::remove("save_data/best_times.txt");
        std::ofstream("save_data/best_times.txt") << "Alex 25 18\n";
        PlayerStore recovered;
        check(!recovered.bestTimeStorageFailed() && recovered.bestTime("Alex",25) == 18,
            "a failed migration must retry successfully without a committed partial marker");
        clear();
        std::ofstream("save_data/players.txt") << "Alex\n";
        std::ofstream("save_data/best_times.txt.bak") << "Alex 25 17\n";
        PlayerStore backup;
        check(backup.bestTime("Alex",25) == 17 && !std::filesystem::exists("save_data/best_times.txt"),
            "migration should read a legacy backup without renaming it");
    }
}

// Run database cases under a separate fixture directory so real scores and other suites remain untouched.
void runDatabaseTests() {
    Fixture fixture;
    migration();
    updatesAndLocks();
    failuresAndRecovery();
    atomicProfileDeletion();
    scoreDatabaseUpgrade();
}
