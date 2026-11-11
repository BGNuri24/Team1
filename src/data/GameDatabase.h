#pragma once
#include "GameMode.h"
#include "StorageResult.h"
#include <filesystem>

// Persist profiles and scores through short-lived connections; migrations run only during
// initialization.
class GameDatabase {
public:
    static constexpr const char* DefaultPath = "save_data/maze_game.db";
    // Retain the requested database path without opening a connection yet. The default points to
    // save_data/maze_game.db; an explicit path lets tests use isolated databases without changing
    // the process working directory.
    explicit GameDatabase(std::filesystem::path Path = DefaultPath);
    // Create the parent directory when needed, open SQLite, and migrate the schema and legacy
    // saves. Mark this instance ready only after migration commits; return the committed profiles
    // or an unavailable result on failure.
    ProfileResult Initialize();
    // Validate the proposed name, then check capacity and duplicates against persisted profiles
    // while holding an immediate write transaction. Insert the profile and return the refreshed
    // list only after commit; report validation and storage failures separately.
    ProfileResult AddPlayer(const std::string& Player);
    // Delete the named profile and all of its scores within one transaction. Return NotFound for
    // a missing profile, preserve both tables if any operation fails, and expose the refreshed
    // profile list only after commit.
    ProfileResult RemovePlayer(const std::string& Player);
    // Look up a score by player, maze size, and mode after validating the key. Success with an
    // empty seconds optional means no score exists; Invalid and Unavailable distinguish bad input
    // from a storage failure.
    BestTimeResult BestTime(const std::string& Player, int CurrentSize, GameMode Mode) const;
    // Validate the completed run and confirm its profile still exists, then compare and write its
    // time under an immediate transaction. Return Updated, Unchanged, Invalid, or StorageError
    // directly so the UI can report the correct outcome.
    RecordResult Record(const std::string& Player, int CurrentSize, float Seconds, GameMode Mode);

private:
    std::filesystem::path Path;
    bool CurrentReady = false;
};
