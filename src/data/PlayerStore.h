#pragma once
#include <string>
#include <vector>
#include "Validation.h"

class PlayerStore {
public:
    // loads save_data/players.txt if it exists
    PlayerStore();                           
    int count() const;
    const std::string& name(int i) const;
    bool contains(const std::string& name) const;
    // saves the file
    bool add(const std::string& name);
    // also saves the file
    bool remove(int i);                      
    float bestTime(const std::string& player, int mazeSize) const;
    bool recordBestTime(const std::string& player, int mazeSize, float seconds);
private:
    struct BestTime {
        std::string player;
        int mazeSize;
        float seconds;
    };

    void load();
    bool save() const;
    void loadBestTimes();
    bool saveBestTimes() const;
    std::vector<std::string> names;
    std::vector<BestTime> bestTimes;
};
