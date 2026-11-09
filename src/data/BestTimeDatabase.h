#pragma once
#include "GameMode.h"
#include <string>
#include <vector>

// Keep profiles and best times in one SQLite database; each operation owns its connection and statements.
class BestTimeDatabase {
public:
    // Initialize the legacy database implementation's schema and import markers inside one
    // transaction. Publish the caller's profiles only after a successful commit; the active game
    // build now uses GameDatabase instead.
    bool initialize(std::vector<std::string>& players);
    // Add a valid unique profile under a write transaction, checking current capacity and
    // cleaning orphaned scores for that name. Refresh the caller's list only on commit; this
    // legacy implementation is not part of the active game target.
    bool addPlayer(const std::string& player, std::vector<std::string>& players);
    // Query the legacy database for the supplied player, size, and mode, returning -1 for a
    // missing result or unavailable storage. Consult storageFailed to distinguish failure; active
    // gameplay uses GameDatabase's explicit result type.
    float bestTime(const std::string& player, int size, GameMode mode) const;
    // Insert a first score or improve a slower result while holding an immediate transaction.
    // This legacy API returns false for both unchanged scores and failures, with storageFailed
    // distinguishing the latter.
    bool record(const std::string& player, int size, float seconds, GameMode mode);
    // Delete a profile and every associated score atomically, publishing the refreshed profile
    // list only on commit. Failure rolls the transaction back and leaves the caller's list
    // unchanged.
    bool removePlayer(const std::string& player, std::vector<std::string>& players);
    // Expose the legacy API's failure flag for its latest operation, separating storage errors
    // from missing scores or unimproved runs. The active GameDatabase API returns these outcomes
    // directly instead.
    bool storageFailed() const { return failed; }
private:
    bool ready = false;
    mutable bool failed = false;
};
