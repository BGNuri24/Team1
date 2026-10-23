#pragma once

enum class PauseAction { NONE, RESUME, RESTART, CHANGE_PLAYER, MAIN_MENU };

class PauseMenu {
public:
    PauseMenu();
    PauseAction update();
    void draw() const;
    void reset();          // highlight "Resume" again each time the pause opens
private:
    int selected;
};