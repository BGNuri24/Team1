#pragma once
#include "../data/PlayerStore.h"
#include <string>
#include <string_view>

class Players {
public:
    enum AddResult { ADDED, INVALID, DUPLICATE, FULL, SaveFailed };

    // Construct the underlying profile store and initialize the active index to zero. An empty or
    // unavailable store is handled by activeName's Guest fallback rather than creating a profile
    // automatically.
    Players();
    // Return the number of cached profiles exposed by PlayerStore. Menu code uses this count to
    // build profile choices; this call does not refresh the database.
    int Count() const;
    // Borrow a profile name by index through PlayerStore, which provides an empty fallback for
    // invalid indices. References to cached entries should not outlive profile-list mutations.
    const std::string& Name(int I) const;
    // Borrow the selected saved profile's name, or return a stable Guest fallback when the index
    // is invalid. Score persistence still requires an actual stored profile.
    const std::string& ActiveName() const;
    // Select the requested profile only when its index is within the cached list. Invalid
    // requests leave the current selection unchanged rather than producing an out-of-range active
    // index.
    void SetActive(int I);
    // Validate the supplied name and ask PlayerStore to add it, translating its explicit storage
    // status into the menu's AddResult. Keep validation, duplicates, capacity, and failed saves
    // distinguishable to the player.
    AddResult Add(std::string_view CurrentName);
    // Remove the indexed profile through persistent storage and adjust activeIndex only after
    // success. Deleting an earlier entry preserves the selected remaining player; an exhausted
    // index falls back to zero.
    bool Remove(int I);
    // Request the active profile's record for a maze size and mode and return both its storage
    // status and optional seconds. A missing score remains distinct from database failure.
    BestTimeResult BestTime(int MazeSize, GameMode Mode = GameMode::Normal) const;
    // Submit the active profile's completed time for its size and mode. The storage layer accepts
    // only a valid first or faster result and returns the exact outcome for the results screen.
    RecordResult RecordWin(int MazeSize, float Seconds, GameMode Mode = GameMode::Normal);

private:
    PlayerStore CurrentStore;
    int ActiveIndex;
};
