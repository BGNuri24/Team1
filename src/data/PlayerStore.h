#pragma once
#include "GameDatabase.h"
#include "GameMode.h"
#include "Validation.h"
#include <string>
#include <vector>

class PlayerStore {
public:
    // Initialize the SQLite database and cache profiles only after initialization succeeds.
    // Retain the startup status for the UI; schema setup and legacy imports are handled by
    // GameDatabase.
    PlayerStore();
    // Return the number of profiles in the last successfully loaded or committed in-memory list.
    // This accessor does not open SQLite or refresh changes made by another game instance.
    int count() const;
    // Borrow the cached name at index i, returning a stable empty string for an invalid index. Do
    // not retain a reference to a vector entry across profile additions or removals.
    const std::string& name(int i) const;
    // Check whether the cached profile list contains the supplied name. This is a local lookup;
    // database write operations independently recheck persisted profiles inside their
    // transactions.
    bool contains(const std::string& name) const;
    // Ask GameDatabase to validate and commit a new profile, then replace the cached list only on
    // success. Return its exact status so duplicate, capacity, validation, and storage failures
    // remain distinguishable.
    StorageStatus add(const std::string& name);
    // Validate the cached index and copy its name before requesting atomic profile-and-score
    // deletion. Replace the cached list only after a successful commit, leaving current memory
    // intact on failure.
    StorageStatus remove(int i);
    // Query the database directly for the supplied player's size-and-mode record. Returning the
    // storage status with an optional time distinguishes a missing score from failure and
    // observes other instances' committed improvements.
    BestTimeResult bestTime(const std::string& player, int mazeSize,
        GameMode mode = GameMode::Normal) const;
    // Forward the complete score key and elapsed seconds to GameDatabase, which validates the
    // profile and compares against the persisted best time. Return the operation's explicit
    // result without maintaining a separate score cache.
    RecordResult recordBestTime(const std::string& player, int mazeSize, float seconds,
        GameMode mode = GameMode::Normal);
    // Return the initialization result captured when this store was constructed. Later operations
    // return their own statuses and do not overwrite this startup diagnostic.
    StorageStatus storageStatus() const { return initialization; }

private:
    std::vector<std::string> names;
    GameDatabase database;
    StorageStatus initialization = StorageStatus::Unavailable;
};
