#include "GameDatabase.h"
#include "DatabaseMigrations.h"
#include "DatabaseQueries.h"
#include "Validation.h"
#include <algorithm>
#include <cmath>
#include <utility>

using namespace storage;

// Retain the requested database path without opening a connection yet. The default points to
// save_data/maze_game.db; an explicit path lets tests use isolated databases without changing the
// process working directory.
GameDatabase::GameDatabase(std::filesystem::path value) : path(std::move(value)) {
}

// Create the parent directory when needed, open SQLite, and migrate the schema and legacy saves.
// Mark this instance ready only after migration commits; return the committed profiles or an
// unavailable result on failure.
ProfileResult GameDatabase::initialize() {
    ready = false;
    std::error_code error;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), error);
    if (error)
        return {};
    Connection connection;
    ProfileResult result;
    if (!connection.open(path) ||
        !migrateDatabase(connection, path.parent_path(), result.players))
        return {};
    ready = true;
    result.status = StorageStatus::Success;
    return result;
}

// Validate the proposed name, then check capacity and duplicates against persisted profiles while
// holding an immediate write transaction. Insert the profile and return the refreshed list only
// after commit; report validation and storage failures separately.
ProfileResult GameDatabase::addPlayer(const std::string& player) {
    if (!isValidPlayerName(player))
        return {StorageStatus::Invalid, {}};
    if (!ready)
        return {};
    Connection connection;
    if (!connection.open(path))
        return {};
    Transaction transaction(connection);
    ProfileResult result;
    if (!transaction.active || !readPlayers(connection, result.players))
        return {};
    if (result.players.size() >= MAX_PLAYERS)
        return {StorageStatus::Full, {}};
    if (std::find(result.players.begin(), result.players.end(), player) != result.players.end())
        return {StorageStatus::Duplicate, {}};
    Statement cleanup, insert;
    if (!cleanup.prepare(connection, "DELETE FROM best_times WHERE player=?1") ||
        !cleanup.text(1, player) || sqlite3_step(cleanup.handle) != SQLITE_DONE ||
        !insert.prepare(connection, "INSERT INTO players(name,sort_order) SELECT "
                                    "?1,COALESCE(MAX(sort_order),-1)+1 FROM players") ||
        !insert.text(1, player) || sqlite3_step(insert.handle) != SQLITE_DONE ||
        !readPlayers(connection, result.players) || !transaction.commit())
        return {};
    result.status = StorageStatus::Success;
    return result;
}

// Delete the named profile and all of its scores within one transaction. Return NotFound for a
// missing profile, preserve both tables if any operation fails, and expose the refreshed profile
// list only after commit.
ProfileResult GameDatabase::removePlayer(const std::string& player) {
    if (!isValidPlayerName(player))
        return {StorageStatus::Invalid, {}};
    if (!ready)
        return {};
    Connection connection;
    if (!connection.open(path))
        return {};
    Transaction transaction(connection);
    Statement profile, scores;
    ProfileResult result;
    if (!transaction.active || !profile.prepare(connection, "DELETE FROM players WHERE name=?1") ||
        !profile.text(1, player) || sqlite3_step(profile.handle) != SQLITE_DONE)
        return {};
    if (!sqlite3_changes(connection.handle))
        return {StorageStatus::NotFound, {}};
    if (!scores.prepare(connection, "DELETE FROM best_times WHERE player=?1") ||
        !scores.text(1, player) || sqlite3_step(scores.handle) != SQLITE_DONE ||
        !readPlayers(connection, result.players) || !transaction.commit())
        return {};
    result.status = StorageStatus::Success;
    return result;
}

// Look up a score by player, maze size, and mode after validating the key. Success with an empty
// seconds optional means no score exists; Invalid and Unavailable distinguish bad input from a
// storage failure.
BestTimeResult GameDatabase::bestTime(const std::string& player, int size, GameMode mode) const {
    if (!isValidPlayerName(player) || !isValidSize(size) || !modeKey(mode))
        return {StorageStatus::Invalid, {}};
    if (!ready)
        return {};
    Connection connection;
    Statement query;
    if (!connection.open(path) ||
        !query.prepare(
            connection,
            "SELECT seconds FROM best_times WHERE player=?1 AND maze_size=?2 AND mode=?3") ||
        !bindKey(query, player, size, mode))
        return {};
    const int result = sqlite3_step(query.handle);
    if (result == SQLITE_DONE)
        return {StorageStatus::Success, {}};
    if (result != SQLITE_ROW)
        return {};
    return {StorageStatus::Success, (float)sqlite3_column_double(query.handle, 0)};
}

// Validate the completed run and confirm its profile still exists, then compare and write its
// time under an immediate transaction. Return Updated, Unchanged, Invalid, or StorageError
// directly so the UI can report the correct outcome.
RecordResult GameDatabase::record(const std::string& player, int size, float seconds,
                                  GameMode mode) {
    if (!isValidPlayerName(player) || !isValidSize(size) || !std::isfinite(seconds) ||
        seconds < 0 || !modeKey(mode))
        return RecordResult::Invalid;
    if (!ready)
        return RecordResult::StorageError;
    Connection connection;
    if (!connection.open(path))
        return RecordResult::StorageError;
    Transaction transaction(connection);
    std::vector<std::string> players;
    if (!transaction.active || !readPlayers(connection, players))
        return RecordResult::StorageError;
    if (std::find(players.begin(), players.end(), player) == players.end())
        return RecordResult::Invalid;
    bool changed = false;
    if (!writeBest(connection, player, size, seconds, mode, changed) || !transaction.commit())
        return RecordResult::StorageError;
    return changed ? RecordResult::Updated : RecordResult::Unchanged;
}
