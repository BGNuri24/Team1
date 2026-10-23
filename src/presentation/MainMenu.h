#pragma once
#include "Screen.h"

class MainMenu {
public:
    MainMenu();
    Screen update();     // returns the next screen (MAIN_MENU = stay here)
    void draw() const;
private:
    int selected;
};