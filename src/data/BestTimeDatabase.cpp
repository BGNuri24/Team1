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
        // Close an owned SQLite connection on scope exit when its handle is nonnull, including a
        // partially successful open. Statements must finish before their connection owner is
        // destroyed.
        ~Connection() { if (handle) sqlite3_close(handle); }
        // Disallow copy construction so a second object cannot duplicate exclusive state or
        // resource handles. Keep a single owner or use an explicitly supported move constructor
        // instead.
        Connection(const Connection&) = delete;
        // Disallow copy assignment so two objects cannot accidentally share exclusive state or
        // resource ownership. Retain the existing owner or use an explicitly supported move
        // operation instead.
        Connection& operator=(const Connection&) = delete;
        // Use the compiler-generated default constructor and the member initializers declared in
        // this type. No external resources are acquired here; later operations establish the
        // object's usable state.
        Connection() = default;

        // Open the legacy implementation's fixed database path using SQLite's UTF-16 Windows API
        // and set a short busy timeout. Return false on setup failure while the owning Connection
        // retains cleanup responsibility.
        bool open() {
            if (sqlite3_open16(DatabasePath.c_str(), &handle) != SQLITE_OK) return false;
            return sqlite3_busy_timeout(handle, 250) == SQLITE_OK;
        }
        // Execute fixed SQL without interpolating user-provided values and report whether SQLite
        // completed successfully. Use prepared Statement bindings for names and other variable
        // data.
        bool execute(const char* sql) {
            return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
        }
    };

    // Finalize prepared statements before their owning connection is closed.
    struct Statement {
        sqlite3_stmt* handle = nullptr;
        // Disallow copy construction so a second object cannot duplicate exclusive state or
        // resource handles. Keep a single owner or use an explicitly supported move constructor
        // instead.
        Statement(const Statement&) = delete;
        // Disallow copy assignment so two objects cannot accidentally share exclusive state or
        // resource ownership. Retain the existing owner or use an explicitly supported move
        // operation instead.
        Statement& operator=(const Statement&) = delete;
        // Use the compiler-generated default constructor and the member initializers declared in
        // this type. No external resources are acquired here; later operations establish the
        // object's usable state.
        Statement() = default;
        // Finalize the owned prepared statement when its scope ends, releasing SQLite resources
        // before the surrounding connection closes. An empty statement needs no cleanup.
        ~Statement() { if (handle) sqlite3_finalize(handle); }

        // Compile fixed SQL against the supplied open connection and retain the resulting
        // statement handle. Return false on preparation failure while the statement owner still
        // handles any allocated cleanup.
        bool prepare(Connection& connection, const char* sql) {
            return sqlite3_prepare_v2(connection.handle, sql, -1, &handle, nullptr) == SQLITE_OK;
        }
        // Bind a string to a one-based prepared-statement parameter using SQLITE_TRANSIENT.
        // SQLite copies the bytes, so the caller's string does not have to survive until the
        // statement is stepped.
        bool text(int index, const std::string& value) {
            return sqlite3_bind_text(handle, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
        }
    };

    // Roll back uncommitted changes automatically when any statement or the commit itself fails.
    struct Transaction {
        Connection& connection;
        bool active;
        // Begin an immediate write transaction on the borrowed connection and record whether
        // acquisition succeeded. The connection must outlive this guard; uncommitted active work
        // is rolled back automatically.
        explicit Transaction(Connection& value) : connection(value), active(value.execute("BEGIN IMMEDIATE")) {}
        // Roll back an active transaction when scope exits before a successful commit. This
        // protects multi-statement profile and score changes from partial publication after an
        // error.
        ~Transaction() { if (active) connection.execute("ROLLBACK"); }
        // Commit an active transaction and clear rollback responsibility only when SQLite reports
        // success. Return false otherwise, leaving the guard active so destruction can roll back
        // the unfinished operation.
        bool commit() {
            if (!active || !connection.execute("COMMIT")) return false;
            active = false;
            return true;
        }
    };

    // Translate the supported GameMode values into the stable strings stored in SQLite. Return
    // nullptr for an unsupported enum value so callers can reject it before binding or executing
    // a query.
    const char* modeKey(GameMode mode) {
        if (mode == GameMode::Normal) return "normal";
        if (mode == GameMode::ZeroGravity) return "zero_gravity";
        return nullptr;
    }

    // Read profiles in sort_order order into a temporary vector, validating names and the
    // player-count limit. Replace the caller's vector only after the complete query succeeds,
    // leaving it unchanged on any failure.
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

    // Query whether the named migration marker exists and place that answer in exists. A missing
    // row is a successful query with exists false; preparation, binding, or stepping errors
    // return false.
    bool hasMarker(Connection& connection, const char* key, bool& exists) {
        Statement marker;
        if (!marker.prepare(connection, "SELECT 1 FROM storage_metadata WHERE key=?1") || !marker.text(1,key)) return false;
        const int status = sqlite3_step(marker.handle);
        if (status != SQLITE_ROW && status != SQLITE_DONE) return false;
        exists = status == SQLITE_ROW;
        return true;
    }

    // Read valid, unique names from save_data/players.txt, falling back to its backup when
    // absent. Respect the player limit and preserve the source; this helper belongs to the older,
    // inactive storage implementation.
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

    // Bind player name, maze size, and mode to parameters 1, 2, and 3 of a prepared statement.
    // Return false for an unsupported mode or any binding error; the same ordering is used by
    // lookups and score writes.
    bool bindKey(Statement& statement, const std::string& player, int size, GameMode mode) {
        const char* key = modeKey(mode);
        return key && statement.text(1, player) &&
            sqlite3_bind_int(statement.handle, 2, size) == SQLITE_OK && statement.text(3, key);
    }

    // Insert a score when its composite key is missing, then update an existing row only when the
    // new time is faster. Report SQL success separately from the changed flag; the caller
    // supplies the surrounding write transaction.
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

    // Import well-formed legacy score rows for existing profiles, retaining the fastest result
    // for each key. Keep the text sources untouched and let the caller commit the import and its
    // marker together.
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

// Initialize the legacy database implementation's schema and import markers inside one
// transaction. Publish the caller's profiles only after a successful commit; the active game
// build now uses GameDatabase instead.
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

// Add a valid unique profile under a write transaction, checking current capacity and cleaning
// orphaned scores for that name. Refresh the caller's list only on commit; this legacy
// implementation is not part of the active game target.
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

// Query the legacy database for the supplied player, size, and mode, returning -1 for a missing
// result or unavailable storage. Consult storageFailed to distinguish failure; active gameplay
// uses GameDatabase's explicit result type.
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

// Insert a first score or improve a slower result while holding an immediate transaction. This
// legacy API returns false for both unchanged scores and failures, with storageFailed
// distinguishing the latter.
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

// Delete a profile and every associated score atomically, publishing the refreshed profile list
// only on commit. Failure rolls the transaction back and leaves the caller's list unchanged.
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
