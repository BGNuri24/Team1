#pragma once
#include "GameMode.h"
#include <string>
#include <vector>

// Own the SQLite best-time operations; connections and statements are scoped inside each call.
class BestTimeDatabase {
public:
    // Create the schema and atomically import valid legacy records once for this database.
    bool initialize(const std::vector<std::string>& players);
    // Query by the complete record key, returning -1 when no result exists or storage is unavailable.
    float bestTime(const std::string& player, int size, GameMode mode) const;
    // Persist a first or faster result in a transaction; return false for unchanged or failed writes.
    bool record(const std::string& player, int size, float seconds, GameMode mode);
    // Remove every result belonging to a profile without changing other players' records.
    bool removePlayer(const std::string& player);
    // Distinguish storage failures from a successful lookup with no result or an unimproved run.
    bool storageFailed() const { return failed; }
private:
    bool ready = false;
    mutable bool failed = false;
};
