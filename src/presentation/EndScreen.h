#pragma once
#include "../logic/Game.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class EndScreen {
public:
    void reset();
    Screen update(float deltaSeconds);
    void draw(const Game& game, const std::string& playerName, float bestTime, bool newBestTime,
              bool bestTimeSaveFailed = false) const;

private:
    float animationTime = 0.0f;
    int selected = 0;
    MenuTypography type;
};
