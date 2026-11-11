#include "PlayerStore.h"
#include <algorithm>
#include <utility>

// Initialize the SQLite database and cache profiles only after initialization succeeds. Retain
// the startup status for the UI; schema setup and legacy imports are handled by GameDatabase.
PlayerStore::PlayerStore() {
    auto Result = CurrentDatabase.Initialize();
    Initialization = Result.Status;
    if (Initialization == StorageStatus::Success)
        Names = std::move(Result.CurrentPlayers);
}

// Return the number of profiles in the last successfully loaded or committed in-memory list. This
// accessor does not open SQLite or refresh changes made by another game instance.
int PlayerStore::Count() const {
    return (int)Names.size();
}

// Borrow the cached name at index i, returning a stable empty string for an invalid index. Do not
// retain a reference to a vector entry across profile additions or removals.
const std::string& PlayerStore::Name(int I) const {
    static const std::string Empty;
    return I >= 0 && I < Count() ? Names[I] : Empty;
}

// Check whether the cached profile list contains the supplied name. This is a local lookup;
// database write operations independently recheck persisted profiles inside their transactions.
bool PlayerStore::Contains(const std::string& CurrentName) const {
    return std::find(Names.begin(), Names.end(), CurrentName) != Names.end();
}

// Ask GameDatabase to validate and commit a new profile, then replace the cached list only on
// success. Return its exact status so duplicate, capacity, validation, and storage failures
// remain distinguishable.
StorageStatus PlayerStore::Add(const std::string& CurrentName) {
    auto Result = CurrentDatabase.AddPlayer(CurrentName);
    if (Result.Status == StorageStatus::Success)
        Names = std::move(Result.CurrentPlayers);
    return Result.Status;
}

// Validate the cached index and copy its name before requesting atomic profile-and-score
// deletion. Replace the cached list only after a successful commit, leaving current memory intact
// on failure.
StorageStatus PlayerStore::Remove(int I) {
    if (I < 0 || I >= Count())
        return StorageStatus::Invalid;
    const std::string RemovedName = Names[I];
    auto Result = CurrentDatabase.RemovePlayer(RemovedName);
    if (Result.Status == StorageStatus::Success)
        Names = std::move(Result.CurrentPlayers);
    return Result.Status;
}

// Query the database directly for the supplied player's size-and-mode record. Returning the
// storage status with an optional time distinguishes a missing score from failure and observes
// other instances' committed improvements.
BestTimeResult PlayerStore::BestTime(const std::string& Player, int MazeSize, GameMode Mode) const {
    return CurrentDatabase.BestTime(Player, MazeSize, Mode);
}

// Forward the complete score key and elapsed seconds to GameDatabase, which validates the profile
// and compares against the persisted best time. Return the operation's explicit result without
// maintaining a separate score cache.
RecordResult PlayerStore::RecordBestTime(const std::string& Player, int MazeSize, float Seconds,
    GameMode Mode) {
    return CurrentDatabase.Record(Player, MazeSize, Seconds, Mode);
}
