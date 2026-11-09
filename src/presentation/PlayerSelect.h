#pragma once
#include "../logic/Players.h"
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"
#include <string>

class PlayerSelect {
public:
    // Borrow the application's Players object and initialize selection, name-entry, and feedback
    // state. The reference does not own profiles; additions and removals go through the shared
    // logic layer.
    explicit PlayerSelect(Players& players);
    // Handle profile selection, validated name entry, and deletion while keeping the active
    // profile in shared Players state. Translate addition failures into feedback and return
    // navigation only after an appropriate confirmation.
    Screen update();
    // Render saved profiles, their active and selected states, delete actions, and the name-entry
    // dialog. Display feedback supplied by update without performing database operations from
    // drawing.
    void draw() const;

private:
    // Count the currently selectable profile entries and available add-profile action. Keyboard
    // navigation uses this value to stay within the list as profiles are added or removed.
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
