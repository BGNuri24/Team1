#pragma once
#include "Screen.h"
#include "MenuBackground.h"

class MainMenu {
public:
    MainMenu();
    // Return the requested destination screen, or MAIN_MENU when no navigation action is selected.
    Screen update();     
    void draw() const;
private:
    int selected;
    MenuBackground background;
};
