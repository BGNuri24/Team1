#include "Application.h"
#include "raylib.h"

Application::Application()
    : playerSelect(players),
      currentSize(0),
      currentSeconds(0),
      bestTime(-1.0f),
      newBestTime(false),
      screen(Screen::MAIN_MENU) {
}

void Application::run() {
    while (screen != Screen::QUIT && !WindowShouldClose()) {
        screen = updateCurrentScreen();
    }
}

Screen Application::updateCurrentScreen() {
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
