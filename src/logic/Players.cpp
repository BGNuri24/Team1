#include "Players.h"

// Load the underlying profile store and initially select its first entry when one exists.
Players::Players() : activeIndex(0) {}

// Expose the number of profiles managed by the persistent store.
int Players::count() const { return store.count(); }

// Delegate indexed name lookup to the store, including its empty-string fallback for an invalid index.
const std::string& Players::name(int i) const { return store.name(i); }

// Resolve the current profile name and use Guest when the selected index does not identify a saved
// profile.
const std::string& Players::activeName() const {
    static const std::string guest = "Guest";
    if (activeIndex < 0 || activeIndex >= store.count()) return guest;
    return store.name(activeIndex);
}

// Change the active profile only when the requested index is inside the saved profile list.
void Players::setActive(int i) {
    if (i >= 0 && i < store.count()) activeIndex = i;
}

// Check name validity, capacity and uniqueness in that order, then report whether saving the new
// profile succeeded.
Players::AddResult Players::add(std::string_view name) {
    if (!isValidPlayerName(name)) return INVALID;
    if (store.count() >= MAX_PLAYERS) return FULL;
    const std::string storedName(name);
    if (store.contains(storedName)) return DUPLICATE;
    return store.add(storedName) ? ADDED : SAVE_FAILED;
}

// Delete a profile and adjust the active index so an earlier deletion does not change which remaining
// player is selected.
bool Players::remove(int i) {
    if (i < 0 || i >= store.count()) return false;
    if (!store.remove(i)) return false;
    if (i < activeIndex) activeIndex--;
    if (activeIndex >= store.count()) activeIndex = 0;
    return true;
}

// Look up the active player's best result for the requested maze size and game mode.
float Players::bestTime(int mazeSize, GameMode mode) const {
    return store.bestTime(activeName(), mazeSize, mode);
}

// Submit the active player's completed run to the store, which accepts only a first or improved valid
// result.
bool Players::recordWin(int mazeSize, float seconds, GameMode mode) {
    return store.recordBestTime(activeName(), mazeSize, seconds, mode);
}

// Forward database failure state so the results screen can distinguish it from an unimproved time.
bool Players::bestTimeStorageFailed() const { return store.bestTimeStorageFailed(); }
