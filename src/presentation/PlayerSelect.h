#pragma once
#include "../logic/Players.h"
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class PlayerSelect {
public:
    explicit PlayerSelect(Players& players);
    // Return LEVEL_SELECT after selecting a profile, MAIN_MENU when navigating back, or
    // PLAYER_SELECT otherwise.
    Screen update();
    void draw() const;

private:
    int optionCount() const;

    Players& players;
    int selected;
    // Track whether the new-player name entry is active instead of the profile list.
    bool typing;
    std::string text;
    // Select the displayed error: 0 clears it, 1 reports invalid entry, 2 reports a duplicate, and
    // 3 reports a save failure.
    int error;
    MenuBackground background;
    MenuTypography type;
};
