#pragma once
#include "GameMode.h"
#include "Sqlite.h"
#include <string>
#include <vector>

namespace Storage {
    // Translate the supported GameMode values into the stable strings stored in SQLite. Return
    // nullptr for an unsupported enum value so callers can reject it before binding or executing
    // a query.
    const char* ModeKey(GameMode Mode);
    // Read profiles in sort_order order into a temporary vector, validating names and the
    // player-count limit. Replace the caller's vector only after the complete query succeeds,
    // leaving it unchanged on any failure.
    bool ReadPlayers(Connection& CurrentConnection, std::vector<std::string>& CurrentPlayers);
    // Bind player name, maze size, and mode to parameters 1, 2, and 3 of a prepared statement.
    // Return false for an unsupported mode or any binding error; the same ordering is used by
    // lookups and score writes.
    bool BindKey(Statement& CurrentStatement, const std::string& Player, int CurrentSize, GameMode Mode);
    // Insert a score when its composite key is missing, then update an existing row only when the
    // new time is faster. Report SQL success separately from the changed flag; the caller
    // supplies the surrounding write transaction.
    bool WriteBest(Connection& CurrentConnection, const std::string& Player, int CurrentSize, float Seconds,
        GameMode Mode, bool& Changed);
}
