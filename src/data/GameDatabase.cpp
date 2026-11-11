#include "GameDatabase.h"
#include "DatabaseMigrations.h"
#include "DatabaseQueries.h"
#include "Validation.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Storage;

// Retain the requested database path without opening a connection yet. The default points to
// save_data/maze_game.db; an explicit path lets tests use isolated databases without changing the
// process working directory.
GameDatabase::GameDatabase(std::filesystem::path Value) : Path(std::move(Value)) {
}

// Create the parent directory when needed, open SQLite, and migrate the schema and legacy saves.
// Mark this instance ready only after migration commits; return the committed profiles or an
// unavailable result on failure.
ProfileResult GameDatabase::Initialize() {
    CurrentReady = false;
    std::error_code Error;
    if (!Path.parent_path().empty())
        std::filesystem::create_directories(Path.parent_path(), Error);
    if (Error)
        return {};
    Connection CurrentConnection;
    ProfileResult Result;
    if (!CurrentConnection.Open(Path) ||
        !MigrateDatabase(CurrentConnection, Path.parent_path(), Result.CurrentPlayers))
        return {};
    CurrentReady = true;
    Result.Status = StorageStatus::Success;
    return Result;
}

// Validate the proposed name, then check capacity and duplicates against persisted profiles while
// holding an immediate write transaction. Insert the profile and return the refreshed list only
// after commit; report validation and storage failures separately.
ProfileResult GameDatabase::AddPlayer(const std::string& Player) {
    if (!IsValidPlayerName(Player))
        return {StorageStatus::Invalid, {}};
    if (!CurrentReady)
        return {};
    Connection CurrentConnection;
    if (!CurrentConnection.Open(Path))
        return {};
    Transaction CurrentTransaction(CurrentConnection);
    ProfileResult Result;
    if (!CurrentTransaction.Active || !ReadPlayers(CurrentConnection, Result.CurrentPlayers))
        return {};
    if (Result.CurrentPlayers.size() >= MaxPlayers)
        return {StorageStatus::Full, {}};
    if (std::find(Result.CurrentPlayers.begin(), Result.CurrentPlayers.end(), Player) != Result.CurrentPlayers.end())
        return {StorageStatus::Duplicate, {}};
    Statement CurrentCleanup, CurrentInsert;
    if (!CurrentCleanup.Prepare(CurrentConnection, "DELETE FROM best_times WHERE player=?1") ||
        !CurrentCleanup.Text(1, Player) || sqlite3_step(CurrentCleanup.CurrentHandle) != SQLITE_DONE ||
        !CurrentInsert.Prepare(CurrentConnection, "INSERT INTO players(name,sort_order) SELECT "
                                    "?1,COALESCE(MAX(sort_order),-1)+1 FROM players") ||
        !CurrentInsert.Text(1, Player) || sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE ||
        !ReadPlayers(CurrentConnection, Result.CurrentPlayers) || !CurrentTransaction.Commit())
        return {};
    Result.Status = StorageStatus::Success;
    return Result;
}

// Delete the named profile and all of its scores within one transaction. Return NotFound for a
// missing profile, preserve both tables if any operation fails, and expose the refreshed profile
// list only after commit.
ProfileResult GameDatabase::RemovePlayer(const std::string& Player) {
    if (!IsValidPlayerName(Player))
        return {StorageStatus::Invalid, {}};
    if (!CurrentReady)
        return {};
    Connection CurrentConnection;
    if (!CurrentConnection.Open(Path))
        return {};
    Transaction CurrentTransaction(CurrentConnection);
    Statement Profile, Scores;
    ProfileResult Result;
    if (!CurrentTransaction.Active || !Profile.Prepare(CurrentConnection, "DELETE FROM players WHERE name=?1") ||
        !Profile.Text(1, Player) || sqlite3_step(Profile.CurrentHandle) != SQLITE_DONE)
        return {};
    if (!sqlite3_changes(CurrentConnection.CurrentHandle))
        return {StorageStatus::NotFound, {}};
    if (!Scores.Prepare(CurrentConnection, "DELETE FROM best_times WHERE player=?1") ||
        !Scores.Text(1, Player) || sqlite3_step(Scores.CurrentHandle) != SQLITE_DONE ||
        !ReadPlayers(CurrentConnection, Result.CurrentPlayers) || !CurrentTransaction.Commit())
        return {};
    Result.Status = StorageStatus::Success;
    return Result;
}

// Look up a score by player, maze size, and mode after validating the key. Success with an empty
// seconds optional means no score exists; Invalid and Unavailable distinguish bad input from a
// storage failure.
BestTimeResult GameDatabase::BestTime(const std::string& Player, int CurrentSize, GameMode Mode) const {
    if (!IsValidPlayerName(Player) || !IsValidSize(CurrentSize) || !ModeKey(Mode))
        return {StorageStatus::Invalid, {}};
    if (!CurrentReady)
        return {};
    Connection CurrentConnection;
    Statement CurrentQuery;
    if (!CurrentConnection.Open(Path) ||
        !CurrentQuery.Prepare(
            CurrentConnection,
            "SELECT seconds FROM best_times WHERE player=?1 AND maze_size=?2 AND mode=?3") ||
        !BindKey(CurrentQuery, Player, CurrentSize, Mode))
        return {};
    const int Result = sqlite3_step(CurrentQuery.CurrentHandle);
    if (Result == SQLITE_DONE)
        return {StorageStatus::Success, {}};
    if (Result != SQLITE_ROW)
        return {};
    return {StorageStatus::Success, (float)sqlite3_column_double(CurrentQuery.CurrentHandle, 0)};
}

// Validate the completed run and confirm its profile still exists, then compare and write its
// time under an immediate transaction. Return Updated, Unchanged, Invalid, or StorageError
// directly so the UI can report the correct outcome.
RecordResult GameDatabase::Record(const std::string& Player, int CurrentSize, float Seconds,
                                  GameMode Mode) {
    if (!IsValidPlayerName(Player) || !IsValidSize(CurrentSize) || !std::isfinite(Seconds) ||
        Seconds < 0 || !ModeKey(Mode))
        return RecordResult::Invalid;
    if (!CurrentReady)
        return RecordResult::StorageError;
    Connection CurrentConnection;
    if (!CurrentConnection.Open(Path))
        return RecordResult::StorageError;
    Transaction CurrentTransaction(CurrentConnection);
    std::vector<std::string> CurrentPlayers;
    if (!CurrentTransaction.Active || !ReadPlayers(CurrentConnection, CurrentPlayers))
        return RecordResult::StorageError;
    if (std::find(CurrentPlayers.begin(), CurrentPlayers.end(), Player) == CurrentPlayers.end())
        return RecordResult::Invalid;
    bool Changed = false;
    if (!WriteBest(CurrentConnection, Player, CurrentSize, Seconds, Mode, Changed) || !CurrentTransaction.Commit())
        return RecordResult::StorageError;
    return Changed ? RecordResult::Updated : RecordResult::Unchanged;
}
