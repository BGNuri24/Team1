#pragma once
#include <string>
#include <vector>
#include "Validation.h"
#include "GameMode.h"
#include "BestTimeDatabase.h"

class PlayerStore {
public:
    // Load profiles from the text file and initialize the SQLite score database with a one-time import.
    PlayerStore();
    int count() const;
    const std::string& name(int i) const;
    bool contains(const std::string& name) const;
    // Add a valid, unique name within the player limit; undo the addition if saving fails.
    bool add(const std::string& name);
    // Remove the indexed player and their records for every mode; restore in-memory data on save
    // failure.
    bool remove(int i);
    float bestTime(const std::string& player, int mazeSize, GameMode mode = GameMode::Normal) const;
    bool recordBestTime(const std::string& player, int mazeSize, float seconds,
        GameMode mode = GameMode::Normal);
    // Distinguish a database error from a run that simply did not improve the existing record.
    bool bestTimeStorageFailed() const;
private:
    void load();
    bool save() const;
    std::vector<std::string> names;
    BestTimeDatabase bestTimes;
};
