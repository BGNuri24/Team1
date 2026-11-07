#pragma once
#include "../data/GameMode.h"
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class LevelSelect {
public:
    LevelSelect();
    // Return PLAYING after a valid choice, PLAYER_SELECT when navigating back, or LEVEL_SELECT
    // while choosing.
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
    MenuBackground background;
    MenuTypography type;
};
