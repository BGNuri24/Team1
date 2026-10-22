#pragma once
#include <string>
#include <string_view>
#include "../data/PlayerStore.h"

const int MAX_NAME_LENGTH = 12;

class Players {
public:
    enum AddResult { ADDED, INVALID, DUPLICATE, FULL, SAVE_FAILED };

    Players();
    int count() const;
    const std::string& name(int i) const;
    // the player who plays now
    const std::string& activeName() const;
    void setActive(int i);
    // validates, then saves
    AddResult add(std::string_view name);
    bool remove(int i);
    float bestTime(int mazeSize) const;
    bool recordWin(int mazeSize, float seconds);
private:
    static bool isValidName(std::string_view name);

    PlayerStore store;
    int activeIndex;
};
