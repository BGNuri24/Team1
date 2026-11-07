#include "Application.h"
#include "Ui.h"
#include "raylib.h"

// Connect menus to shared profiles and settings, initialize the selected-run values, and start at
// the main menu.
Application::Application()
    : playerSelect(players), pauseMenu(settings), currentSize(0), currentSeconds(0),
      currentMode(GameMode::Normal), bestTime(-1.0f), newBestTime(false),
      screen(Screen::MAIN_MENU) {
    prepareTheme();
}

// Preview the next run's theme throughout menu navigation and consume it only when a run starts.
void Application::prepareTheme() {
    const MazeTheme selected = themeSelector.preview();
    if (MenuTheme::current() != selected)
        MenuTheme::apply(selected);
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
}
