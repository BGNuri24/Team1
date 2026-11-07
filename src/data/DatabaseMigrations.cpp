#include "DatabaseMigrations.h"
#include "DatabaseQueries.h"
#include "Validation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <utility>
using namespace storage;
namespace {
// Treat the metadata key as a migration marker only after its surrounding transaction commits.
bool hasMarker(Connection& connection, const char* key, bool& exists) {
    Statement marker;
    if (!marker.prepare(connection, "SELECT 1 FROM storage_metadata WHERE key=?1") ||
        !marker.text(1, key))
        return false;
    const int status = sqlite3_step(marker.handle);
    if (status != SQLITE_ROW && status != SQLITE_DONE)
        return false;
    exists = status == SQLITE_ROW;
    return true;
}
// Import only the player's own legacy files beside this database, with backups as a fallback.
bool legacySource(const std::filesystem::path& directory, const char* filename,
                  std::filesystem::path& source, bool& exists) {
    const std::filesystem::path candidates[] = {directory / filename,
                                              directory / (std::string(filename) + ".bak")};
    std::error_code error;
    for (const auto& candidate : candidates) {
        source = candidate;
        exists = std::filesystem::exists(source, error);
        if (error)
            return false;
        if (exists)
            return true;
    }
    exists = false;
    return true;
}
// Import profile order without rewriting either legacy file, and fail on an unreadable source.
bool readLegacyPlayers(const std::filesystem::path& directory, std::vector<std::string>& players) {
    std::filesystem::path source;
    bool exists = false;
    if (!legacySource(directory, "players.txt", source, exists))
        return false;
    players.clear();
    if (!exists)
        return true;
    std::ifstream input(source);
    if (!input)
        return false;
    std::string name;
    while (std::getline(input, name)) {
        if (!name.empty() && name.back() == '\r')
            name.pop_back();
        if (players.size() < MAX_PLAYERS && isValidPlayerName(name) &&
            std::find(players.begin(), players.end(), name) == players.end())
            players.push_back(name);
    }
    return !input.bad();
}
// Import only valid records for existing profiles; leave the source and any .bak file untouched.
bool importLegacy(Connection& connection, const std::filesystem::path& directory,
                  const std::vector<std::string>& players) {
    std::filesystem::path source;
    bool exists = false;
    if (!legacySource(directory, "best_times.txt", source, exists))
        return false;
    if (!exists)
        return true;
    std::ifstream input(source);
    if (!input)
        return false;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        fields.imbue(std::locale::classic());
        std::string player, modeName, extra;
        int size = 0;
        float seconds = 0;
        GameMode mode = GameMode::Normal;
        if (!(fields >> player >> size >> seconds))
            continue;
        if (fields >> modeName) {
            if (modeName == "zero_gravity")
                mode = GameMode::ZeroGravity;
            else if (modeName != "normal")
                continue;
            if (fields >> extra)
                continue;
        }
        if (!isValidPlayerName(player) || !isValidSize(size) || !std::isfinite(seconds) ||
            seconds < 0 || std::find(players.begin(), players.end(), player) == players.end())
            continue;
        bool changed = false;
        if (!writeBest(connection, player, size, seconds, mode, changed))
            return false;
    }
    return !input.bad();
}
}
// Commit schema changes and legacy import markers together; keep this work outside runtime queries.
bool migrateDatabase(Connection& connection, const std::filesystem::path& directory,
                     std::vector<std::string>& players) {
    Transaction transaction(connection);
    if (!transaction.active ||
        !connection.execute(
            "CREATE TABLE IF NOT EXISTS best_times("
            "player TEXT NOT NULL CHECK(length(player) BETWEEN 1 AND 12),"
            "maze_size INTEGER NOT NULL CHECK(maze_size BETWEEN 10 AND 50),"
            "mode TEXT NOT NULL CHECK(mode IN ('normal','zero_gravity')),"
            "seconds REAL NOT NULL CHECK(seconds>=0),"
            "PRIMARY KEY(player,maze_size,mode));"
            "CREATE TABLE IF NOT EXISTS storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
            "CREATE TABLE IF NOT EXISTS players("
            "name TEXT PRIMARY KEY NOT NULL CHECK(length(name) BETWEEN 1 AND 12 AND name NOT GLOB "
            "'*[^A-Za-z0-9_]*'),"
            "sort_order INTEGER NOT NULL UNIQUE CHECK(sort_order>=0));"))
        return false;
    bool profilesImported = false;
    if (!hasMarker(connection, "legacy_players_imported", profilesImported))
        return false;
    if (!profilesImported) {
        std::vector<std::string> legacyPlayers;
        if (!readLegacyPlayers(directory, legacyPlayers))
            return false;
        for (int index = 0; index < (int)legacyPlayers.size(); ++index) {
            Statement insert;
            if (!insert.prepare(connection, "INSERT INTO players(name,sort_order) VALUES(?1,?2)") ||
                !insert.text(1, legacyPlayers[index]) ||
                sqlite3_bind_int(insert.handle, 2, index) != SQLITE_OK ||
                sqlite3_step(insert.handle) != SQLITE_DONE)
                return false;
        }
        if (!connection.execute(
                "INSERT INTO storage_metadata(key,value) VALUES('legacy_players_imported','1')"))
            return false;
    }
    std::vector<std::string> storedPlayers;
    if (!readPlayers(connection, storedPlayers))
        return false;
    bool imported = false;
    if (!hasMarker(connection, "legacy_best_times_imported", imported))
        return false;
    if (!imported &&
        (!importLegacy(connection, directory, storedPlayers) ||
         !connection.execute(
             "INSERT INTO storage_metadata(key,value) VALUES('legacy_best_times_imported','1')")))
        return false;
    if (!transaction.commit())
        return false;
    players = std::move(storedPlayers);
    return true;
}
