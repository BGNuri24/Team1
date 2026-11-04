#include "PlayerStore.h"
#include <algorithm>

// Load both profiles and best times from SQLite, with a one-time import handled inside its transaction.
PlayerStore::PlayerStore() { bestTimes.initialize(names); }

// Return the number of profiles in the last successfully loaded or committed profile list.
int PlayerStore::count() const { return (int)names.size(); }

// Return a stable empty string for invalid indices rather than exposing an out-of-range reference.
const std::string& PlayerStore::name(int i) const {
    static const std::string empty;
    return i >= 0 && i < count() ? names[i] : empty;
}

// Check the current in-memory list; database writes also validate the latest persisted state.
bool PlayerStore::contains(const std::string& name) const {
    return std::find(names.begin(),names.end(),name) != names.end();
}

// Apply local validation before committing the new profile and publishing its refreshed list.
bool PlayerStore::add(const std::string& name) {
    if (!isValidPlayerName(name) || count() >= MAX_PLAYERS || contains(name)) return false;
    return bestTimes.addPlayer(name,names);
}

// Delete the selected profile and all its scores in one SQLite transaction, leaving memory unchanged on failure.
bool PlayerStore::remove(int i) {
    if (i < 0 || i >= count()) return false;
    const std::string removedName = names[i];
    return bestTimes.removePlayer(removedName,names);
}

// Read persisted scores directly so improvements made through another store are visible immediately.
float PlayerStore::bestTime(const std::string& player, int mazeSize, GameMode mode) const {
    return contains(player) ? bestTimes.bestTime(player,mazeSize,mode) : -1.0f;
}

// Reject an unknown local profile and let SQLite recheck profile existence and the current best result.
bool PlayerStore::recordBestTime(const std::string& player, int mazeSize, float seconds, GameMode mode) {
    return contains(player) && bestTimes.record(player,mazeSize,seconds,mode);
}

// Distinguish unavailable storage from a run that legitimately did not improve its previous record.
bool PlayerStore::bestTimeStorageFailed() const { return bestTimes.storageFailed(); }
