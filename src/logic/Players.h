#pragma once
#include "../data/PlayerStore.h"
#include <string>
#include <string_view>

class Players {
public:
    enum AddResult { ADDED, INVALID, DUPLICATE, FULL, SAVE_FAILED };

    Players();
    int count() const;
    const std::string& name(int i) const;
    // Return the selected profile's name, or Guest when no valid profile is selected.
    const std::string& activeName() const;
    void setActive(int i);
    // Validate the name and player limit before saving; return the specific reason an addition
    // fails.
    AddResult add(std::string_view name);
    bool remove(int i);
    BestTimeResult bestTime(int mazeSize, GameMode mode = GameMode::Normal) const;
    RecordResult recordWin(int mazeSize, float seconds, GameMode mode = GameMode::Normal);

private:
    PlayerStore store;
    int activeIndex;
};
