#pragma once
#include "../data/GameMode.h"
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class LevelSelect {
public:
    // Initialize the selected option and custom-level entry state before the screen is first
    // shown. Chosen size, timer, and mode are exposed through accessors after update confirms a
    // level.
    LevelSelect();
    // Handle level-card navigation, Zero Gravity selection, and custom-size text entry. Validate
    // a custom size before requesting PLAYING and retain an explanatory error when confirmation
    // is invalid.
    Screen update();
    // Render preset cards, the Zero Gravity option, and the custom-size dialog using shared theme
    // and typography. Display the current selection and any validation feedback without changing
    // chosen run parameters.
    void draw() const;
    // Return the selected maze side length for Application::startGame. Read this after update
    // confirms play; the accessor does not validate a new value or regenerate a maze.
    int size() const;
    // Return the selected run's time allowance in seconds, using the preset or validated
    // custom-size formula. Application captures this value when creating a new Game.
    float seconds() const;
    // Return the mode associated with the selected level, distinguishing Zero Gravity from Normal
    // presets and custom play. This read-only value determines which gameplay mechanics the new
    // run enables.
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
