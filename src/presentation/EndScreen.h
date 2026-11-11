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
    void Reset();
    // Advance result-screen animation and handle keyboard or pointer actions for retry and
    // navigation. Return the requested Screen while the finished Game itself remains frozen.
    Screen Update(float DeltaSeconds);
    // Render the frozen run's outcome, distinguishing victory, timeout, and obstacle contact.
    // Show the player's time, best-time status, save failures, and selected navigation action
    // without attempting persistence.
    void Draw(const Game& CurrentGame, const std::string& PlayerName, float BestTime, bool NewBestTime,
              bool BestTimeSaveFailed = false) const;

private:
    float AnimationTime = 0.0f;
    int CurrentSelected = 0;
    MenuTypography Type;
};
