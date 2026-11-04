#pragma once
#include "GameMode.h"
#include <string>
#include <vector>

// Keep profiles and best times in one SQLite database; each operation owns its connection and statements.
class BestTimeDatabase {
public:
    // Create the schema and atomically import valid legacy records once for this database.
    bool initialize(std::vector<std::string>& players);
    // Add a unique profile within the limit and publish the refreshed list only after commit succeeds.
    bool addPlayer(const std::string& player, std::vector<std::string>& players);
    // Query by the complete record key, returning -1 when no result exists or storage is unavailable.
    float bestTime(const std::string& player, int size, GameMode mode) const;
    // Persist a first or faster result in a transaction; return false for unchanged or failed writes.
    bool record(const std::string& player, int size, float seconds, GameMode mode);
    // Commit the profile deletion and all its score deletions together, then refresh the caller's list.
    bool removePlayer(const std::string& player, std::vector<std::string>& players);
    // Distinguish storage failures from a successful lookup with no result or an unimproved run.
    bool storageFailed() const { return failed; }
private:
    bool ready = false;
    mutable bool failed = false;
};
