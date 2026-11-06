#pragma once
#include "MenuBackground.h"
#include "Screen.h"
#include "raylib.h"
#include <array>

class MainMenu {
public:
    MainMenu();
    ~MainMenu();
    MainMenu(const MainMenu&) = delete;
    MainMenu& operator=(const MainMenu&) = delete;
    // Return the requested destination screen, or MAIN_MENU when no navigation action is selected.
    Screen update();
    void draw() const;

private:
    void refreshFonts() const;
    int selected;
    MenuBackground background;
    // Rebuild glyphs only when their displayed pixel sizes change during a window resize.
    mutable std::array<int, 3> fontPixels{};
    mutable Font titleFont{};
    mutable Font menuFont{};
    mutable Font detailFont{};
};
