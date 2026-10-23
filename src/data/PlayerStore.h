#pragma once
#include <string>
#include <vector>

const int MAX_PLAYERS = 5;

class PlayerStore {
public:
    PlayerStore();                           // loads players.txt if it exists
    int count() const;
    const std::string& name(int i) const;
    bool contains(const std::string& name) const;
    bool add(const std::string& name);       // saves the file
    bool remove(int i);                      // saves the file
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