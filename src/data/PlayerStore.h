#pragma once
#include "GameDatabase.h"
#include "GameMode.h"
#include "Validation.h"
#include <string>
#include <vector>

class PlayerStore {
public:
    // Load SQLite profiles and scores, importing legacy text files only during the initial
    // migration.
    PlayerStore();
    int count() const;
    const std::string& name(int i) const;
    bool contains(const std::string& name) const;
    // Add a valid, unique name within the player limit; undo the addition if saving fails.
    StorageStatus add(const std::string& name);
    // Remove the indexed player and their records for every mode; restore in-memory data on save
    // failure.
    StorageStatus remove(int i);
    BestTimeResult bestTime(const std::string& player, int mazeSize,
        GameMode mode = GameMode::Normal) const;
    RecordResult recordBestTime(const std::string& player, int mazeSize, float seconds,
        GameMode mode = GameMode::Normal);
    // Report startup availability; subsequent operations return their own status without mutating
    // it.
    StorageStatus storageStatus() const { return initialization; }

private:
    std::vector<std::string> names;
    GameDatabase database;
    StorageStatus initialization = StorageStatus::Unavailable;
};
