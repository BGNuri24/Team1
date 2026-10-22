#pragma once
#include <string>
#include "Screen.h"
#include "../logic/Game.h"

class EndScreen {
public:
    Screen update();
    void draw(const Game& game, const std::string& playerName,
        float bestTime, bool newBestTime) const;
};