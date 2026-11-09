#include "Players.h"

// Construct the underlying profile store and initialize the active index to zero. An empty or
// unavailable store is handled by activeName's Guest fallback rather than creating a profile
// automatically.
Players::Players() : activeIndex(0) {
}

// Return the number of cached profiles exposed by PlayerStore. Menu code uses this count to build
// profile choices; this call does not refresh the database.
int Players::count() const {
    return store.count();
}

// Borrow a profile name by index through PlayerStore, which provides an empty fallback for
// invalid indices. References to cached entries should not outlive profile-list mutations.
const std::string& Players::name(int i) const {
    return store.name(i);
}

// Borrow the selected saved profile's name, or return a stable Guest fallback when the index is
// invalid. Score persistence still requires an actual stored profile.
const std::string& Players::activeName() const {
    static const std::string guest = "Guest";
    if (activeIndex < 0 || activeIndex >= store.count())
        return guest;
    return store.name(activeIndex);
}

// Select the requested profile only when its index is within the cached list. Invalid requests
// leave the current selection unchanged rather than producing an out-of-range active index.
void Players::setActive(int i) {
    if (i >= 0 && i < store.count())
        activeIndex = i;
}

// Validate the supplied name and ask PlayerStore to add it, translating its explicit storage
// status into the menu's AddResult. Keep validation, duplicates, capacity, and failed saves
// distinguishable to the player.
Players::AddResult Players::add(std::string_view name) {
    if (!isValidPlayerName(name))
        return INVALID;
    const std::string storedName(name);
    switch (store.add(storedName)) {
    case StorageStatus::Success:
        return ADDED;
    case StorageStatus::Invalid:
        return INVALID;
    case StorageStatus::Duplicate:
        return DUPLICATE;
    case StorageStatus::Full:
        return FULL;
    default:
        return SAVE_FAILED;
    }
}

// Remove the indexed profile through persistent storage and adjust activeIndex only after
// success. Deleting an earlier entry preserves the selected remaining player; an exhausted index
// falls back to zero.
bool Players::remove(int i) {
    if (i < 0 || i >= store.count())
        return false;
    if (store.remove(i) != StorageStatus::Success)
        return false;
    if (i < activeIndex)
        activeIndex--;
    if (activeIndex >= store.count())
        activeIndex = 0;
    return true;
}

// Request the active profile's record for a maze size and mode and return both its storage status
// and optional seconds. A missing score remains distinct from database failure.
BestTimeResult Players::bestTime(int mazeSize, GameMode mode) const {
    return store.bestTime(activeName(), mazeSize, mode);
}

// Submit the active profile's completed time for its size and mode. The storage layer accepts
// only a valid first or faster result and returns the exact outcome for the results screen.
RecordResult Players::recordWin(int mazeSize, float seconds, GameMode mode) {
    return store.recordBestTime(activeName(), mazeSize, seconds, mode);
}
