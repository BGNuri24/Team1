#pragma once
#include "../logic/GameSettings.h"

enum class PauseAction { NONE, RESUME, RESTART, CHANGE_PLAYER, MAIN_MENU };

class PauseMenu {
public:
    explicit PauseMenu(GameSettings& settings);
    PauseAction update();
    void draw() const;
    // highlight "Resume" again each time the pause opens
    void reset();
private:
    void persistSensitivity();
    GameSettings& settings;
    int selected;
    bool dragging = false;
    bool saveFailed = false;
};
