#pragma once
#include "../logic/GameSettings.h"

enum class PauseAction { NONE, RESUME, RESTART, CHANGE_PLAYER, MAIN_MENU };

class PauseMenu {
public:
    explicit PauseMenu(GameSettings& settings);
    PauseAction update();
    void draw() const;
    // Select Resume and cancel slider dragging when the pause menu is opened again.
    void reset();
private:
    void persistSensitivity();
    GameSettings& settings;
    int selected;
    bool dragging = false;
    bool saveFailed = false;
};
