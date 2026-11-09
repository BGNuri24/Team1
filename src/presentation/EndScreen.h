#pragma once
#include "../logic/Game.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class EndScreen {
public:
    // Restart the result-card entrance animation and select its first action. Application calls
    // this once when a run finishes so a previous result's animation and selection do not carry
    // over.
    void reset();
    // Advance result-screen animation and handle keyboard or pointer actions for retry and
    // navigation. Return the requested Screen while the finished Game itself remains frozen.
    Screen update(float deltaSeconds);
    // Render the frozen run's outcome, distinguishing victory, timeout, and obstacle contact.
    // Show the player's time, best-time status, save failures, and selected navigation action
    // without attempting persistence.
    void draw(const Game& game, const std::string& playerName, float bestTime, bool newBestTime,
              bool bestTimeSaveFailed = false) const;

private:
    float animationTime = 0.0f;
    int selected = 0;
    MenuTypography type;
};
