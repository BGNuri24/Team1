#include "Application.h"
#include "Ui.h"
#include "raylib.h"
#include <optional>

namespace {
// Identify screens that participate in animated corridor menu navigation. Gameplay, pause,
// results, generation, and quit use separate handling rather than being treated as ordinary menu
// pages.
bool menuScreen(Screen screen) {
    return screen == Screen::MAIN_MENU || screen == Screen::CONTROLS ||
           screen == Screen::PLAYER_SELECT || screen == Screen::LEVEL_SELECT;
}

// Translate an application screen into its corridor-art destination. Return an empty optional for
// screens without a menu stop so gameplay and overlays do not trigger unrelated backdrop
// navigation.
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

// Connect profile selection to shared players and pause controls to shared settings, then
// initialize run parameters and the main-menu state. Prepare the active palette and place the
// shared corridor at its entrance.
Application::Application()
    : playerSelect(players), pauseMenu(settings), currentSize(0), currentSeconds(0),
      currentMode(GameMode::Normal), bestTime(-1.0f), newBestTime(false),
      screen(Screen::MAIN_MENU) {
    prepareTheme();
    menuBackdrop.snap(MenuPlace::Entrance);
}

// Keep the menu palette synchronized with ActiveMazeTheme, applying it only when the selected
// theme differs. This avoids unnecessary theme changes while menus are repeatedly updated.
void Application::prepareTheme() {
    if (MenuTheme::current() != ActiveMazeTheme)
        MenuTheme::apply(ActiveMazeTheme);
}

// Dispatch frames until a screen requests Quit or the operating system requests window closure.
// Each handler returns the next application state and performs that screen's update and drawing.
void Application::run() {
    while (screen != Screen::QUIT && !WindowShouldClose()) {
        screen = updateCurrentScreen();
    }
}

// Dispatch one frame to the active state, giving an in-progress corridor journey priority over
// normal screen input. Route menu changes through travel animation and snap returns from gameplay
// to the correct menu stop.
Screen Application::updateCurrentScreen() {
    if (!game)
        prepareTheme();
    if (menuBackdrop.moving())
        return updateMenuTravel();
    // Dispatch the current screen through an immediately invoked lambda and retain its requested
    // destination. The outer function can then apply shared corridor-navigation rules to every
    // handler's result.
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
