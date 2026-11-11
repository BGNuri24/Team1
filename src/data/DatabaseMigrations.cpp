#include "DatabaseMigrations.h"
#include "DatabaseQueries.h"
#include "Validation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <utility>
using namespace Storage;
namespace {
// Query whether the named migration marker exists and place that answer in exists. A missing row
// is a successful query with exists false; preparation, binding, or stepping errors return false.
bool HasMarker(Connection& CurrentConnection, const char* Key, bool& Exists) {
    Statement Marker;
    if (!Marker.Prepare(CurrentConnection, "SELECT 1 FROM storage_metadata WHERE key=?1") ||
        !Marker.Text(1, Key))
        return false;
    const int Status = sqlite3_step(Marker.CurrentHandle);
    if (Status != SQLITE_ROW && Status != SQLITE_DONE)
        return false;
    Exists = Status == SQLITE_ROW;
    return true;
}
// Resolve a legacy file only inside the database's own directory, using its .bak file when the
// primary file is absent. Report existence separately from filesystem errors and never search
// packaged assets for personal saves.
bool LegacySource(const std::filesystem::path& Directory, const char* Filename,
                  std::filesystem::path& Source, bool& Exists) {
    const std::filesystem::path Candidates[] = {Directory / Filename,
                                              Directory / (std::string(Filename) + ".bak")};
    std::error_code Error;
    for (const auto& Candidate : Candidates) {
        Source = Candidate;
        Exists = std::filesystem::exists(Source, Error);
        if (Error)
            return false;
        if (Exists)
            return true;
    }
    Exists = false;
    return true;
}
// Load valid, unique profile names in file order from the selected legacy source, respecting
// MAX_PLAYERS. Missing files represent an empty import; unreadable files fail migration without
// rewriting the source.
bool ReadLegacyPlayers(const std::filesystem::path& Directory, std::vector<std::string>& CurrentPlayers) {
    std::filesystem::path Source;
    bool Exists = false;
    if (!LegacySource(Directory, "players.txt", Source, Exists))
        return false;
    CurrentPlayers.clear();
    if (!Exists)
        return true;
    std::ifstream CurrentInput(Source);
    if (!CurrentInput)
        return false;
    std::string CurrentName;
    while (std::getline(CurrentInput, CurrentName)) {
        if (!CurrentName.empty() && CurrentName.back() == '\r')
            CurrentName.pop_back();
        if (CurrentPlayers.size() < MaxPlayers && IsValidPlayerName(CurrentName) &&
            std::find(CurrentPlayers.begin(), CurrentPlayers.end(), CurrentName) == CurrentPlayers.end())
            CurrentPlayers.push_back(CurrentName);
    }
    return !CurrentInput.bad();
}
// Read legacy score rows for known profiles, rejecting malformed names, sizes, modes, and times.
// Save only valid improvements through the caller's transaction, preserving the original text
// file and backup.
bool ImportLegacy(Connection& CurrentConnection, const std::filesystem::path& Directory,
                  const std::vector<std::string>& CurrentPlayers) {
    std::filesystem::path Source;
    bool Exists = false;
    if (!LegacySource(Directory, "best_times.txt", Source, Exists))
        return false;
    if (!Exists)
        return true;
    std::ifstream CurrentInput(Source);
    if (!CurrentInput)
        return false;
    std::string Line;
    while (std::getline(CurrentInput, Line)) {
        std::istringstream Fields(Line);
        Fields.imbue(std::locale::classic());
        std::string Player, ModeName, Extra;
        int CurrentSize = 0;
        float Seconds = 0;
        GameMode Mode = GameMode::Normal;
        if (!(Fields >> Player >> CurrentSize >> Seconds))
            continue;
        if (Fields >> ModeName) {
            if (ModeName == "zero_gravity")
                Mode = GameMode::ZeroGravity;
            else if (ModeName != "normal")
                continue;
            if (Fields >> Extra)
                continue;
        }
        if (!IsValidPlayerName(Player) || !IsValidSize(CurrentSize) || !std::isfinite(Seconds) ||
            Seconds < 0 || std::find(CurrentPlayers.begin(), CurrentPlayers.end(), Player) == CurrentPlayers.end())
            continue;
        bool Changed = false;
        if (!WriteBest(CurrentConnection, Player, CurrentSize, Seconds, Mode, Changed))
            return false;
    }
    return !CurrentInput.bad();
}
}
// Create the storage schema and import legacy profiles and scores inside one immediate
// transaction. Publish the profile list and import markers only on commit, so an interrupted or
// failed migration can be retried.
bool MigrateDatabase(Connection& CurrentConnection, const std::filesystem::path& Directory,
                     std::vector<std::string>& CurrentPlayers) {
    Transaction CurrentTransaction(CurrentConnection);
    if (!CurrentTransaction.Active ||
        !CurrentConnection.Execute(
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
    bool ProfilesImported = false;
    if (!HasMarker(CurrentConnection, "legacy_players_imported", ProfilesImported))
        return false;
    if (!ProfilesImported) {
        std::vector<std::string> LegacyPlayers;
        if (!ReadLegacyPlayers(Directory, LegacyPlayers))
            return false;
        for (int Index = 0; Index < (int)LegacyPlayers.size(); ++Index) {
            Statement CurrentInsert;
            if (!CurrentInsert.Prepare(CurrentConnection, "INSERT INTO players(name,sort_order) VALUES(?1,?2)") ||
                !CurrentInsert.Text(1, LegacyPlayers[Index]) ||
                sqlite3_bind_int(CurrentInsert.CurrentHandle, 2, Index) != SQLITE_OK ||
                sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE)
                return false;
        }
        if (!CurrentConnection.Execute(
                "INSERT INTO storage_metadata(key,value) VALUES('legacy_players_imported','1')"))
            return false;
    }
    std::vector<std::string> StoredPlayers;
    if (!ReadPlayers(CurrentConnection, StoredPlayers))
        return false;
    bool Imported = false;
    if (!HasMarker(CurrentConnection, "legacy_best_times_imported", Imported))
        return false;
    if (!Imported &&
        (!ImportLegacy(CurrentConnection, Directory, StoredPlayers) ||
         !CurrentConnection.Execute(
             "INSERT INTO storage_metadata(key,value) VALUES('legacy_best_times_imported','1')")))
        return false;
    if (!CurrentTransaction.Commit())
        return false;
    CurrentPlayers = std::move(StoredPlayers);
    return true;
}
