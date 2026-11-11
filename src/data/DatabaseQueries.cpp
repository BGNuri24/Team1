#include "DatabaseQueries.h"
#include "Validation.h"
#include <utility>
namespace Storage {
    // Translate the supported GameMode values into the stable strings stored in SQLite. Return
    // nullptr for an unsupported enum value so callers can reject it before binding or executing
    // a query.
    const char* ModeKey(GameMode Mode) {
        if (Mode == GameMode::Normal)
            return "normal";
        if (Mode == GameMode::ZeroGravity)
            return "zero_gravity";
        return nullptr;
    }
    // Read profiles in sort_order order into a temporary vector, validating names and the
    // player-count limit. Replace the caller's vector only after the complete query succeeds,
    // leaving it unchanged on any failure.
    bool ReadPlayers(Connection& CurrentConnection, std::vector<std::string>& CurrentPlayers) {
        Statement CurrentQuery;
        if (!CurrentQuery.Prepare(CurrentConnection, "SELECT name FROM players ORDER BY sort_order"))
            return false;
        std::vector<std::string> Result;
        int Status = SQLITE_ROW;
        while ((Status = sqlite3_step(CurrentQuery.CurrentHandle)) == SQLITE_ROW) {
            const auto Value = sqlite3_column_text(CurrentQuery.CurrentHandle, 0);
            if (!Value)
                return false;
            Result.emplace_back((const char*)Value);
            if (!IsValidPlayerName(Result.back()) || Result.size() > MaxPlayers)
                return false;
        }
        if (Status != SQLITE_DONE)
            return false;
        CurrentPlayers = std::move(Result);
        return true;
    }
    // Bind player name, maze size, and mode to parameters 1, 2, and 3 of a prepared statement.
    // Return false for an unsupported mode or any binding error; the same ordering is used by
    // lookups and score writes.
    bool BindKey(Statement& CurrentStatement, const std::string& Player, int CurrentSize, GameMode Mode) {
        const char* Key = ModeKey(Mode);
        return Key && CurrentStatement.Text(1, Player) &&
            sqlite3_bind_int(CurrentStatement.CurrentHandle, 2, CurrentSize) == SQLITE_OK && CurrentStatement.Text(3, Key);
    }
    // Insert a score when its composite key is missing, then update an existing row only when the
    // new time is faster. Report SQL success separately from the changed flag; the caller
    // supplies the surrounding write transaction.
    bool WriteBest(Connection& CurrentConnection, const std::string& Player, int CurrentSize, float Seconds,
        GameMode Mode, bool& Changed) {
        Statement CurrentInsert;
        if (!CurrentInsert.Prepare(CurrentConnection,
            "INSERT OR IGNORE INTO best_times(player,maze_size,mode,seconds) "
            "VALUES(?1,?2,?3,?4)") ||
            !BindKey(CurrentInsert, Player, CurrentSize, Mode) ||
            sqlite3_bind_double(CurrentInsert.CurrentHandle, 4, Seconds) != SQLITE_OK ||
            sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE)
            return false;
        Changed = sqlite3_changes(CurrentConnection.CurrentHandle) != 0;
        Statement Improve;
        if (!Improve.Prepare(CurrentConnection, "UPDATE best_times SET seconds=?4 WHERE player=?1 AND "
            "maze_size=?2 AND mode=?3 AND seconds>?4") ||
            !BindKey(Improve, Player, CurrentSize, Mode) ||
            sqlite3_bind_double(Improve.CurrentHandle, 4, Seconds) != SQLITE_OK ||
            sqlite3_step(Improve.CurrentHandle) != SQLITE_DONE)
            return false;
        Changed |= sqlite3_changes(CurrentConnection.CurrentHandle) != 0;
        return true;
    }
}
