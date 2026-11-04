#pragma once
#include <string>
#include "Screen.h"
#include "../logic/Game.h"

class EndScreen {
public:
    void reset();
    Screen update(float deltaSeconds);
    void draw(const Game& game, const std::string& playerName,
        float bestTime, bool newBestTime, bool bestTimeSaveFailed = false) const;

private:
    float animationTime = 0.0f;
};
