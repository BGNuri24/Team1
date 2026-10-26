#include "Application.h"
#include "Input.h"
#include "raylib.h"

template <typename Draw>
static void drawFrame(Draw draw) {
    BeginDrawing();
    draw();
    EndDrawing();
}

template <typename Menu>
static Screen updateMenu(Menu& menu) {
    Screen next = menu.update();
    drawFrame([&] { menu.draw(); });
    return next;
}

Screen Application::updateMainMenu() { return updateMenu(mainMenu); }
Screen Application::updateControls() { return updateMenu(controls); }
Screen Application::updatePlayerSelect() { return updateMenu(playerSelect); }

Screen Application::startGame() {
    game = std::make_unique<Game>(currentSize, currentSeconds, currentMode);
    generationScreen.begin(*game);
    return Screen::GENERATING;
}

Screen Application::updateLevelSelect() {
    Screen next = levelSelect.update();
    if (next == Screen::PLAYING) {
        currentSize = levelSelect.size();
        currentSeconds = levelSelect.seconds();
        currentMode = levelSelect.mode();
        next = startGame();
    }

    drawFrame([&] { levelSelect.draw(); });
    return next;
}

Screen Application::updateGenerating() {
    bool complete = generationScreen.update(*game);
    drawFrame([&] { generationScreen.draw(*game); });
    if (complete) {
        DisableCursor();
        return Screen::PLAYING;
    }
    return Screen::GENERATING;
}

Screen Application::updatePlaying() {
    game->update(readInput());
    bool pause = IsKeyPressed(KEY_ESCAPE);
    drawFrame([&] { renderer.draw(*game); });

    if (game->finished()) {
        EnableCursor();
        newBestTime = game->won() && players.recordWin(currentSize, game->timeTaken(), currentMode);
        bestTime = players.bestTime(currentSize, currentMode);
        return Screen::END;
    }
    if (pause) {
        EnableCursor();
        pauseMenu.reset();
        return Screen::PAUSED;
    }
    return Screen::PLAYING;
}

Screen Application::updatePaused() {
    PauseAction action = pauseMenu.update();
    drawFrame([&] {
        renderer.draw(*game);
        pauseMenu.draw();
        });

    switch (action) {
    case PauseAction::RESUME:
        DisableCursor();
        return Screen::PLAYING;
    case PauseAction::RESTART:
        return startGame();
    case PauseAction::CHANGE_PLAYER:
        game.reset();
        return Screen::PLAYER_SELECT;
    case PauseAction::MAIN_MENU:
        game.reset();
        return Screen::MAIN_MENU;
    case PauseAction::NONE:
        return Screen::PAUSED;
    }
    return Screen::PAUSED;
}

Screen Application::updateEnd() {
    Screen next = endScreen.update();
    drawFrame([&] {
        renderer.draw(*game);
        endScreen.draw(*game, players.activeName(), bestTime, newBestTime);
        });

    if (next != Screen::END) {
        game.reset();
    }
    return next;
}
