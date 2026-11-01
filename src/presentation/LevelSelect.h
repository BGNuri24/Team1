#pragma once
#include <string>
#include "Screen.h"
#include "../data/GameMode.h"

class LevelSelect {
public:
    LevelSelect();
    // Return PLAYING after a valid choice, MAIN_MENU when navigating back, or LEVEL_SELECT while
    // choosing.
    Screen update();
    void draw() const;
    // Return the selected square maze's side length in cells.
    int size() const;
    // Return the selected level's countdown duration in seconds.
    float seconds() const;
    GameMode mode() const;
private:
    int selected;
    // Keep custom-size entry active until the player confirms a valid value or cancels.
    bool typing;
    // Store the custom-size digits as text until confirmation validates and converts them.
    std::string text;
    bool showError;
    int chosenSize;
    float chosenSeconds;
    GameMode chosenMode;
};
