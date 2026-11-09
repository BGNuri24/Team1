#pragma once
#include "MenuBackground.h"
#include "Screen.h"
#include "raylib.h"
#include <array>

class MainMenu {
public:
    // Initialize the main-menu selection and font cache, loading typefaces for the current window
    // scale. The menu owns its custom font handles and releases them in its destructor.
    MainMenu();
    // Release the title, option, and detail font handles while leaving raylib's shared default
    // font intact. Font cleanup must happen before the application closes the graphics context.
    ~MainMenu();
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    MainMenu(const MainMenu&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    MainMenu& operator=(const MainMenu&) = delete;
    // Process keyboard and pointer navigation for the main options and return a requested
    // application screen. Maintain selection between frames without starting a game or changing
    // shared application ownership directly.
    Screen update();
    // Render the main-menu foreground, wordmark, selectable options, and supporting decorations
    // over the shared corridor. Refresh scale-dependent fonts as needed while leaving navigation
    // decisions to update.
    void draw() const;

private:
    // Rebuild title and supporting font atlases when the effective menu scale changes. Reuse
    // cached fonts at an unchanged size and release old custom atlases before replacement.
    void refreshFonts() const;
    int selected;
    MenuBackground background;
    // Rebuild glyphs only when their displayed pixel sizes change during a window resize.
    mutable std::array<int, 3> fontPixels{};
    mutable Font titleFont{};
    mutable Font menuFont{};
    mutable Font detailFont{};
};
