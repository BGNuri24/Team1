#include "Application.h"
#include "Ui.h"
#include "raylib.h"
#include <optional>

namespace {
bool menuScreen(Screen screen) {
    return screen == Screen::MAIN_MENU || screen == Screen::CONTROLS ||
           screen == Screen::PLAYER_SELECT || screen == Screen::LEVEL_SELECT;
}

std::optional<MenuPlace> corridorPlace(Screen screen) {
    switch (screen) {
    case Screen::MAIN_MENU:
        return MenuPlace::Entrance;
    case Screen::CONTROLS:
        return MenuPlace::Controls;
    case Screen::PLAYER_SELECT:
        return MenuPlace::Players;
    case Screen::LEVEL_SELECT:
        return MenuPlace::Levels;
    case Screen::GENERATING:
        return MenuPlace::Preparing;
    case Screen::QUIT:
        return MenuPlace::Exit;
    default:
        return std::nullopt;
    }
}
}

// Connect menus to shared profiles and settings, initialize the selected-run values, and start at
// the main menu.
Application::Application()
    : playerSelect(players), pauseMenu(settings), currentSize(0), currentSeconds(0),
      currentMode(GameMode::Normal), bestTime(-1.0f), newBestTime(false),
      screen(Screen::MAIN_MENU) {
    prepareTheme();
    menuBackdrop.snap(MenuPlace::Entrance);
}

// Keep every menu on the same curated palette used by new runs and retries.
void Application::prepareTheme() {
    if (MenuTheme::current() != ActiveMazeTheme)
        MenuTheme::apply(ActiveMazeTheme);
}

// Process the current screen repeatedly until navigation requests Quit or the operating system
// closes the window.
void Application::run() {
    while (screen != Screen::QUIT && !WindowShouldClose()) {
        screen = updateCurrentScreen();
    }
}

// Dispatch one frame to the handler for the active application state and return its requested next
// state.
Screen Application::updateCurrentScreen() {
    if (!game)
        prepareTheme();
    if (menuBackdrop.moving())
        return updateMenuTravel();
    const Screen next = [&] {
        switch (screen) {
        case Screen::MAIN_MENU:
            return updateMainMenu();
        case Screen::CONTROLS:
            return updateControls();
        case Screen::PLAYER_SELECT:
            return updatePlayerSelect();
        case Screen::LEVEL_SELECT:
            return updateLevelSelect();
        case Screen::GENERATING:
            return updateGenerating();
        case Screen::PLAYING:
            return updatePlaying();
        case Screen::PAUSED:
            return updatePaused();
        case Screen::END:
            return updateEnd();
        case Screen::QUIT:
            return Screen::QUIT;
        }
        return Screen::QUIT;
    }();
    const std::optional<MenuPlace> destination = corridorPlace(next);
    if (menuScreen(screen)) {
        if (next != screen && destination && menuBackdrop.navigate(*destination)) {
            pendingMenu = next;
            travelFirstFrame = true;
            return screen;
        }
        // Name/custom-size dialogs make a smaller move within their current corridor stop.
        if (menuBackdrop.moving()) {
            pendingMenu = next;
            travelFirstFrame = true;
            return screen;
        }
    } else if (destination && next != screen) {
        // Returning from a paused or completed run starts at the correct menu stop.
        menuBackdrop.snap(*destination);
    }
    return next;
}
