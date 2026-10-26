#pragma once
#include <string>
#include "Screen.h"
#include "../data/GameMode.h"

class LevelSelect {
public:
    LevelSelect();
    // PLAYING = level chosen, MAIN_MENU = back, LEVEL_SELECT = stay
    Screen update();
    void draw() const;
    // chosen maze size
    int size() const;
    // chosen time limit
    float seconds() const;
    GameMode mode() const;
private:
    int selected;
    // true while the player types a custom size
    bool typing;
    // the typed digits
    std::string text;
    bool showError;
    int chosenSize;
    float chosenSeconds;
    GameMode chosenMode;
};
