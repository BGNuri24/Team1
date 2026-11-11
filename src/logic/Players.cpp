#include "Players.h"

// Construct the underlying profile store and initialize the active index to zero. An empty or
// unavailable store is handled by activeName's Guest fallback rather than creating a profile
// automatically.
Players::Players() : ActiveIndex(0) {
}

// Return the number of cached profiles exposed by PlayerStore. Menu code uses this count to build
// profile choices; this call does not refresh the database.
int Players::Count() const {
    return CurrentStore.Count();
}

// Borrow a profile name by index through PlayerStore, which provides an empty fallback for
// invalid indices. References to cached entries should not outlive profile-list mutations.
const std::string& Players::Name(int I) const {
    return CurrentStore.Name(I);
}

// Borrow the selected saved profile's name, or return a stable Guest fallback when the index is
// invalid. Score persistence still requires an actual stored profile.
const std::string& Players::ActiveName() const {
    static const std::string CurrentGuest = "Guest";
    if (ActiveIndex < 0 || ActiveIndex >= CurrentStore.Count())
        return CurrentGuest;
    return CurrentStore.Name(ActiveIndex);
}

// Select the requested profile only when its index is within the cached list. Invalid requests
// leave the current selection unchanged rather than producing an out-of-range active index.
void Players::SetActive(int I) {
    if (I >= 0 && I < CurrentStore.Count())
        ActiveIndex = I;
}

// Validate the supplied name and ask PlayerStore to add it, translating its explicit storage
// status into the menu's AddResult. Keep validation, duplicates, capacity, and failed saves
// distinguishable to the player.
Players::AddResult Players::Add(std::string_view CurrentName) {
    if (!IsValidPlayerName(CurrentName))
        return INVALID;
    const std::string StoredName(CurrentName);
    switch (CurrentStore.Add(StoredName)) {
    case StorageStatus::Success:
        return ADDED;
    case StorageStatus::Invalid:
        return INVALID;
    case StorageStatus::Duplicate:
        return DUPLICATE;
    case StorageStatus::Full:
        return FULL;
    default:
        return SaveFailed;
    }
}

// Remove the indexed profile through persistent storage and adjust activeIndex only after
// success. Deleting an earlier entry preserves the selected remaining player; an exhausted index
// falls back to zero.
bool Players::Remove(int I) {
    if (I < 0 || I >= CurrentStore.Count())
        return false;
    if (CurrentStore.Remove(I) != StorageStatus::Success)
        return false;
    if (I < ActiveIndex)
        ActiveIndex--;
    if (ActiveIndex >= CurrentStore.Count())
        ActiveIndex = 0;
    return true;
}

// Request the active profile's record for a maze size and mode and return both its storage status
// and optional seconds. A missing score remains distinct from database failure.
BestTimeResult Players::BestTime(int MazeSize, GameMode Mode) const {
    return CurrentStore.BestTime(ActiveName(), MazeSize, Mode);
}

// Submit the active profile's completed time for its size and mode. The storage layer accepts
// only a valid first or faster result and returns the exact outcome for the results screen.
RecordResult Players::RecordWin(int MazeSize, float Seconds, GameMode Mode) {
    return CurrentStore.RecordBestTime(ActiveName(), MazeSize, Seconds, Mode);
}
