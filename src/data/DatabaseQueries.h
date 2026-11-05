#pragma once
#include "GameMode.h"
#include "Sqlite.h"
#include <string>
#include <vector>

namespace storage {
    const char* modeKey(GameMode mode);
    bool readPlayers(Connection& connection, std::vector<std::string>& players);
    bool bindKey(Statement& statement, const std::string& player, int size, GameMode mode);
    bool writeBest(Connection& connection, const std::string& player, int size, float seconds,
        GameMode mode, bool& changed);
}
