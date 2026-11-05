#include "PlayerStore.h"
#include <algorithm>
#include <utility>

// Load both profiles and best times from SQLite, with a one-time import handled inside its
// transaction.
PlayerStore::PlayerStore() {
    auto result = database.initialize();
    initialization = result.status;
    if (initialization == StorageStatus::Success)
        names = std::move(result.players);
}

// Return the number of profiles in the last successfully loaded or committed profile list.
int PlayerStore::count() const {
    return (int)names.size();
}

// Return a stable empty string for invalid indices rather than exposing an out-of-range reference.
const std::string& PlayerStore::name(int i) const {
    static const std::string empty;
    return i >= 0 && i < count() ? names[i] : empty;
}

// Check the current in-memory list; database writes also validate the latest persisted state.
bool PlayerStore::contains(const std::string& name) const {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// Apply local validation before committing the new profile and publishing its refreshed list.
StorageStatus PlayerStore::add(const std::string& name) {
    auto result = database.addPlayer(name);
    if (result.status == StorageStatus::Success)
        names = std::move(result.players);
    return result.status;
}

// Delete the selected profile and all its scores in one SQLite transaction, leaving memory
// unchanged on failure.
StorageStatus PlayerStore::remove(int i) {
    if (i < 0 || i >= count())
        return StorageStatus::Invalid;
    const std::string removedName = names[i];
    auto result = database.removePlayer(removedName);
    if (result.status == StorageStatus::Success)
        names = std::move(result.players);
    return result.status;
}

// Read persisted scores directly so improvements made through another store are visible
// immediately.
BestTimeResult PlayerStore::bestTime(const std::string& player, int mazeSize, GameMode mode) const {
    return database.bestTime(player, mazeSize, mode);
}

// Reject an unknown local profile and let SQLite recheck profile existence and the current best
// result.
RecordResult PlayerStore::recordBestTime(const std::string& player, int mazeSize, float seconds,
    GameMode mode) {
    return database.record(player, mazeSize, seconds, mode);
}
