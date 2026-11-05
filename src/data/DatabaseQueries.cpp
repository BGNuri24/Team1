#include "DatabaseQueries.h"
#include "Validation.h"
#include <utility>
namespace storage {
    // Store a readable mode key while rejecting enum values that are not supported by the game.
    const char* modeKey(GameMode mode) {
        if (mode == GameMode::Normal)
            return "normal";
        if (mode == GameMode::ZeroGravity)
            return "zero_gravity";
        return nullptr;
    }
    // Read stable profile order without exposing partial results when a query fails.
    bool readPlayers(Connection& connection, std::vector<std::string>& players) {
        Statement query;
        if (!query.prepare(connection, "SELECT name FROM players ORDER BY sort_order"))
            return false;
        std::vector<std::string> result;
        int status = SQLITE_ROW;
        while ((status = sqlite3_step(query.handle)) == SQLITE_ROW) {
            const auto value = sqlite3_column_text(query.handle, 0);
            if (!value)
                return false;
            result.emplace_back((const char*)value);
            if (!isValidPlayerName(result.back()) || result.size() > MAX_PLAYERS)
                return false;
        }
        if (status != SQLITE_DONE)
            return false;
        players = std::move(result);
        return true;
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
        if (!insert.prepare(connection,
            "INSERT OR IGNORE INTO best_times(player,maze_size,mode,seconds) "
            "VALUES(?1,?2,?3,?4)") ||
            !bindKey(insert, player, size, mode) ||
            sqlite3_bind_double(insert.handle, 4, seconds) != SQLITE_OK ||
            sqlite3_step(insert.handle) != SQLITE_DONE)
            return false;
        changed = sqlite3_changes(connection.handle) != 0;
        Statement improve;
        if (!improve.prepare(connection, "UPDATE best_times SET seconds=?4 WHERE player=?1 AND "
            "maze_size=?2 AND mode=?3 AND seconds>?4") ||
            !bindKey(improve, player, size, mode) ||
            sqlite3_bind_double(improve.handle, 4, seconds) != SQLITE_OK ||
            sqlite3_step(improve.handle) != SQLITE_DONE)
            return false;
        changed |= sqlite3_changes(connection.handle) != 0;
        return true;
    }
}
