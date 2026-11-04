#include "BestTimeDatabase.h"
#include "Validation.h"
#include <winsqlite/winsqlite3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <utility>

namespace {
    const std::filesystem::path DatabasePath = "save_data/maze_game.db";

    // Close every connection on scope exit, including one whose open operation only partly succeeded.
    struct Connection {
        sqlite3* handle = nullptr;
        ~Connection() { if (handle) sqlite3_close(handle); }
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection() = default;

        // Use UTF-16 for Windows paths and wait briefly for another game's write transaction.
        bool open() {
            if (sqlite3_open16(DatabasePath.c_str(), &handle) != SQLITE_OK) return false;
            return sqlite3_busy_timeout(handle, 250) == SQLITE_OK;
        }
        // Execute fixed SQL without interpolating any profile names or other user-provided values.
        bool execute(const char* sql) {
            return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
        }
    };

    // Finalize prepared statements before their owning connection is closed.
    struct Statement {
        sqlite3_stmt* handle = nullptr;
        Statement(const Statement&) = delete;
        Statement& operator=(const Statement&) = delete;
        Statement() = default;
        ~Statement() { if (handle) sqlite3_finalize(handle); }

        // Compile a fixed query and retain its handle only for the surrounding operation.
        bool prepare(Connection& connection, const char* sql) {
            return sqlite3_prepare_v2(connection.handle, sql, -1, &handle, nullptr) == SQLITE_OK;
        }
        // Copy text into the statement so caller-owned strings need not outlive query execution.
        bool text(int index, const std::string& value) {
            return sqlite3_bind_text(handle, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
        }
    };

    // Roll back uncommitted changes automatically when any statement or the commit itself fails.
    struct Transaction {
        Connection& connection;
        bool active;
        explicit Transaction(Connection& value) : connection(value), active(value.execute("BEGIN IMMEDIATE")) {}
        ~Transaction() { if (active) connection.execute("ROLLBACK"); }
        // Publish the complete write operation; retain rollback responsibility if publication fails.
        bool commit() {
            if (!active || !connection.execute("COMMIT")) return false;
            active = false;
            return true;
        }
    };

    // Store a readable mode key while rejecting enum values that are not supported by the game.
    const char* modeKey(GameMode mode) {
        if (mode == GameMode::Normal) return "normal";
        if (mode == GameMode::ZeroGravity) return "zero_gravity";
        return nullptr;
    }

    // Read stable profile order without exposing partial results when a query fails.
    bool readPlayers(Connection& connection, std::vector<std::string>& players) {
        Statement query;
        if (!query.prepare(connection, "SELECT name FROM players ORDER BY sort_order")) return false;
        std::vector<std::string> result;
        int status = SQLITE_ROW;
        while ((status = sqlite3_step(query.handle)) == SQLITE_ROW) {
            const auto value = sqlite3_column_text(query.handle, 0);
            if (!value) return false;
            result.emplace_back((const char*)value);
            if (!isValidPlayerName(result.back()) || result.size() > MAX_PLAYERS) return false;
        }
        if (status != SQLITE_DONE) return false;
        players = std::move(result);
        return true;
    }

    // Treat the metadata key as a migration marker only after its surrounding transaction commits.
    bool hasMarker(Connection& connection, const char* key, bool& exists) {
        Statement marker;
        if (!marker.prepare(connection, "SELECT 1 FROM storage_metadata WHERE key=?1") || !marker.text(1,key)) return false;
        const int status = sqlite3_step(marker.handle);
        if (status != SQLITE_ROW && status != SQLITE_DONE) return false;
        exists = status == SQLITE_ROW;
        return true;
    }

    // Import profile order without rewriting either legacy file, and fail on an unreadable source.
    bool readLegacyPlayers(std::vector<std::string>& players) {
        std::filesystem::path source = "save_data/players.txt";
        std::error_code error;
        bool exists = std::filesystem::exists(source,error);
        if (error) return false;
        if (!exists) {
            source += ".bak";
            exists = std::filesystem::exists(source,error);
            if (error) return false;
        }
        players.clear();
        if (!exists) return true;
        std::ifstream input(source);
        if (!input) return false;
        std::string name;
        while (std::getline(input,name)) {
            if (!name.empty() && name.back() == '\r') name.pop_back();
            if (players.size() < MAX_PLAYERS && isValidPlayerName(name) &&
                std::find(players.begin(),players.end(),name) == players.end()) players.push_back(name);
        }
        return !input.bad();
    }

    // Bind the complete unique key consistently for selects, inserts and improvements.
    bool bindKey(Statement& statement, const std::string& player, int size, GameMode mode) {
        const char* key = modeKey(mode);
        return key && statement.text(1, player) &&
            sqlite3_bind_int(statement.handle, 2, size) == SQLITE_OK && statement.text(3, key);
    }

    // Insert a missing key and improve an existing key under the caller's write transaction.
    bool writeBest(Connection& connection, const std::string& player, int size, float seconds,
        GameMode mode, bool& changed) {
        Statement insert;
        if (!insert.prepare(connection, "INSERT OR IGNORE INTO best_times(player,maze_size,mode,seconds) VALUES(?1,?2,?3,?4)") ||
            !bindKey(insert, player, size, mode) ||
            sqlite3_bind_double(insert.handle, 4, seconds) != SQLITE_OK ||
            sqlite3_step(insert.handle) != SQLITE_DONE) return false;
        changed = sqlite3_changes(connection.handle) != 0;
        Statement improve;
        if (!improve.prepare(connection, "UPDATE best_times SET seconds=?4 WHERE player=?1 AND maze_size=?2 AND mode=?3 AND seconds>?4") ||
            !bindKey(improve, player, size, mode) ||
            sqlite3_bind_double(improve.handle, 4, seconds) != SQLITE_OK ||
            sqlite3_step(improve.handle) != SQLITE_DONE) return false;
        changed |= sqlite3_changes(connection.handle) != 0;
        return true;
    }

    // Import only valid records for existing profiles; leave the source and any .bak file untouched.
    bool importLegacy(Connection& connection, const std::vector<std::string>& players) {
        std::filesystem::path source = "save_data/best_times.txt";
        std::error_code error;
        bool exists = std::filesystem::exists(source, error);
        if (error) return false;
        if (!exists) {
            source += ".bak";
            exists = std::filesystem::exists(source, error);
            if (error) return false;
        }
        if (!exists) return true;
        std::ifstream input(source);
        if (!input) return false;
        std::string line;
        while (std::getline(input, line)) {
            std::istringstream fields(line);
            fields.imbue(std::locale::classic());
            std::string player, modeName, extra;
            int size = 0;
            float seconds = 0;
            GameMode mode = GameMode::Normal;
            if (!(fields >> player >> size >> seconds)) continue;
            if (fields >> modeName) {
                if (modeName == "zero_gravity") mode = GameMode::ZeroGravity;
                else if (modeName != "normal") continue;
                if (fields >> extra) continue;
            }
            if (!isValidPlayerName(player) || !isValidSize(size) || !std::isfinite(seconds) || seconds < 0 ||
                std::find(players.begin(), players.end(), player) == players.end()) continue;
            bool changed = false;
            if (!writeBest(connection, player, size, seconds, mode, changed)) return false;
        }
        return !input.bad();
    }
}

// Commit the schema, imported results and import marker together so an interrupted migration can retry.
bool BestTimeDatabase::initialize(std::vector<std::string>& players) {
    ready = false;
    failed = true;
    std::error_code error;
    std::filesystem::create_directories(DatabasePath.parent_path(), error);
    if (error) return false;
    Connection connection;
    if (!connection.open()) return false;
    Transaction transaction(connection);
    if (!transaction.active || !connection.execute(
        "CREATE TABLE IF NOT EXISTS best_times("
        "player TEXT NOT NULL CHECK(length(player) BETWEEN 1 AND 12),"
        "maze_size INTEGER NOT NULL CHECK(maze_size BETWEEN 10 AND 50),"
        "mode TEXT NOT NULL CHECK(mode IN ('normal','zero_gravity')),"
        "seconds REAL NOT NULL CHECK(seconds>=0),"
        "PRIMARY KEY(player,maze_size,mode));"
        "CREATE TABLE IF NOT EXISTS storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS players("
        "name TEXT PRIMARY KEY NOT NULL CHECK(length(name) BETWEEN 1 AND 12 AND name NOT GLOB '*[^A-Za-z0-9_]*'),"
        "sort_order INTEGER NOT NULL UNIQUE CHECK(sort_order>=0));")) return false;
    bool profilesImported = false;
    if (!hasMarker(connection,"legacy_players_imported",profilesImported)) return false;
    if (!profilesImported) {
        std::vector<std::string> legacyPlayers;
        if (!readLegacyPlayers(legacyPlayers)) return false;
        for (int index = 0; index < (int)legacyPlayers.size(); ++index) {
            Statement insert;
            if (!insert.prepare(connection,"INSERT INTO players(name,sort_order) VALUES(?1,?2)") ||
                !insert.text(1,legacyPlayers[index]) || sqlite3_bind_int(insert.handle,2,index) != SQLITE_OK ||
                sqlite3_step(insert.handle) != SQLITE_DONE) return false;
        }
        if (!connection.execute("INSERT INTO storage_metadata(key,value) VALUES('legacy_players_imported','1')")) return false;
    }
    std::vector<std::string> storedPlayers;
    if (!readPlayers(connection,storedPlayers)) return false;
    bool imported = false;
    if (!hasMarker(connection,"legacy_best_times_imported",imported)) return false;
    if (!imported && (!importLegacy(connection, storedPlayers) || !connection.execute(
        "INSERT INTO storage_metadata(key,value) VALUES('legacy_best_times_imported','1')"))) return false;
    if (!transaction.commit()) return false;
    players = std::move(storedPlayers);
    ready = true;
    failed = false;
    return true;
}

// Serialize capacity checks, orphan cleanup and insertion so two store instances cannot exceed the limit.
bool BestTimeDatabase::addPlayer(const std::string& player, std::vector<std::string>& players) {
    failed = false;
    if (!isValidPlayerName(player)) return false;
    failed = true;
    if (!ready) return false;
    Connection connection;
    if (!connection.open()) return false;
    Transaction transaction(connection);
    std::vector<std::string> current;
    if (!transaction.active || !readPlayers(connection,current)) return false;
    if (current.size() >= MAX_PLAYERS || std::find(current.begin(),current.end(),player) != current.end()) {
        failed = false;
        return false;
    }
    Statement cleanup, insert;
    if (!cleanup.prepare(connection,"DELETE FROM best_times WHERE player=?1") || !cleanup.text(1,player) ||
        sqlite3_step(cleanup.handle) != SQLITE_DONE ||
        !insert.prepare(connection,"INSERT INTO players(name,sort_order) SELECT ?1,COALESCE(MAX(sort_order),-1)+1 FROM players") ||
        !insert.text(1,player) || sqlite3_step(insert.handle) != SQLITE_DONE ||
        !readPlayers(connection,current) || !transaction.commit()) return false;
    players = std::move(current);
    failed = false;
    return true;
}

// Query current persisted state rather than a cached copy that another game instance could invalidate.
float BestTimeDatabase::bestTime(const std::string& player, int size, GameMode mode) const {
    failed = true;
    if (!ready) return -1;
    Connection connection;
    Statement query;
    if (!connection.open() || !query.prepare(connection,
        "SELECT seconds FROM best_times WHERE player=?1 AND maze_size=?2 AND mode=?3") ||
        !bindKey(query, player, size, mode)) return -1;
    const int result = sqlite3_step(query.handle);
    if (result != SQLITE_ROW && result != SQLITE_DONE) return -1;
    failed = false;
    return result == SQLITE_ROW ? (float)sqlite3_column_double(query.handle, 0) : -1;
}

// Protect read/modify/write ordering with an immediate transaction so a slower concurrent run cannot win.
bool BestTimeDatabase::record(const std::string& player, int size, float seconds, GameMode mode) {
    failed = false;
    if (!isValidPlayerName(player) || !isValidSize(size) || !std::isfinite(seconds) || seconds < 0 || !modeKey(mode)) return false;
    failed = true;
    if (!ready) return false;
    Connection connection;
    if (!connection.open()) return false;
    Transaction transaction(connection);
    std::vector<std::string> players;
    if (!transaction.active || !readPlayers(connection,players)) return false;
    if (std::find(players.begin(),players.end(),player) == players.end()) {
        failed = false;
        return false;
    }
    bool changed = false;
    if (!transaction.active || !writeBest(connection, player, size, seconds, mode, changed) ||
        !transaction.commit()) return false;
    failed = false;
    return changed;
}

// Roll back both deletions if either statement fails; publish the in-memory profile list only on commit.
bool BestTimeDatabase::removePlayer(const std::string& player, std::vector<std::string>& players) {
    failed = true;
    if (!ready) return false;
    Connection connection;
    if (!connection.open()) return false;
    Transaction transaction(connection);
    Statement profile, scores;
    std::vector<std::string> remaining;
    if (!transaction.active || !profile.prepare(connection,"DELETE FROM players WHERE name=?1") ||
        !profile.text(1,player) || sqlite3_step(profile.handle) != SQLITE_DONE) return false;
    if (!sqlite3_changes(connection.handle)) { failed = false; return false; }
    if (!scores.prepare(connection,"DELETE FROM best_times WHERE player=?1") || !scores.text(1,player) ||
        sqlite3_step(scores.handle) != SQLITE_DONE || !readPlayers(connection,remaining) ||
        !transaction.commit()) return false;
    players = std::move(remaining);
    failed = false;
    return true;
}
