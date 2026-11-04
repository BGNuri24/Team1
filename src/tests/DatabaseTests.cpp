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
        check(interrupted.bestTimeStorageFailed(), "an unreadable import source must fail initialization");
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
}
