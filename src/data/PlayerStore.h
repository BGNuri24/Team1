#pragma once
#include <string>
#include <vector>
#include "Validation.h"
#include "GameMode.h"

class PlayerStore {
public:
    // Load player names and best-time records from save_data, recovering backup files when available.
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
private:
    struct BestTime {
        std::string player;
        int mazeSize;
        float seconds;
        GameMode mode = GameMode::Normal;
    };

    void load();
    bool save() const;
    void loadBestTimes();
    bool saveBestTimes() const;
    std::vector<std::string> names;
    std::vector<BestTime> bestTimes;
};
