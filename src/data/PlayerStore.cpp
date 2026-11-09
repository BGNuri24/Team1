#include "PlayerStore.h"
#include <algorithm>
#include <utility>

// Initialize the SQLite database and cache profiles only after initialization succeeds. Retain
// the startup status for the UI; schema setup and legacy imports are handled by GameDatabase.
PlayerStore::PlayerStore() {
    auto result = database.initialize();
    initialization = result.status;
    if (initialization == StorageStatus::Success)
        names = std::move(result.players);
}

// Return the number of profiles in the last successfully loaded or committed in-memory list. This
// accessor does not open SQLite or refresh changes made by another game instance.
int PlayerStore::count() const {
    return (int)names.size();
}

// Borrow the cached name at index i, returning a stable empty string for an invalid index. Do not
// retain a reference to a vector entry across profile additions or removals.
const std::string& PlayerStore::name(int i) const {
    static const std::string empty;
    return i >= 0 && i < count() ? names[i] : empty;
}

// Check whether the cached profile list contains the supplied name. This is a local lookup;
// database write operations independently recheck persisted profiles inside their transactions.
bool PlayerStore::contains(const std::string& name) const {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// Ask GameDatabase to validate and commit a new profile, then replace the cached list only on
// success. Return its exact status so duplicate, capacity, validation, and storage failures
// remain distinguishable.
StorageStatus PlayerStore::add(const std::string& name) {
    auto result = database.addPlayer(name);
    if (result.status == StorageStatus::Success)
        names = std::move(result.players);
    return result.status;
}

// Validate the cached index and copy its name before requesting atomic profile-and-score
// deletion. Replace the cached list only after a successful commit, leaving current memory intact
// on failure.
StorageStatus PlayerStore::remove(int i) {
    if (i < 0 || i >= count())
        return StorageStatus::Invalid;
    const std::string removedName = names[i];
    auto result = database.removePlayer(removedName);
    if (result.status == StorageStatus::Success)
        names = std::move(result.players);
    return result.status;
}

// Query the database directly for the supplied player's size-and-mode record. Returning the
// storage status with an optional time distinguishes a missing score from failure and observes
// other instances' committed improvements.
BestTimeResult PlayerStore::bestTime(const std::string& player, int mazeSize, GameMode mode) const {
    return database.bestTime(player, mazeSize, mode);
}

// Forward the complete score key and elapsed seconds to GameDatabase, which validates the profile
// and compares against the persisted best time. Return the operation's explicit result without
// maintaining a separate score cache.
RecordResult PlayerStore::recordBestTime(const std::string& player, int mazeSize, float seconds,
    GameMode mode) {
    return database.record(player, mazeSize, seconds, mode);
}
