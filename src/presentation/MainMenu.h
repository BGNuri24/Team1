#pragma once
#include "Screen.h"

class MainMenu {
public:
    MainMenu();
    // returns the next screen (MAIN_MENU = stay here)
    Screen update();     
    void draw() const;
private:
    int selected;
};