#include "Application.h"
#include "Input.h"
#include "raylib.h"

Screen Application::updateMainMenu() {
    Screen next = mainMenu.update();
    BeginDrawing();
    mainMenu.draw();
    EndDrawing();
    return next;
}

Screen Application::updateControls() {
    Screen next = controls.update();
    BeginDrawing();
    controls.draw();
    EndDrawing();
    return next;
}

Screen Application::updatePlayerSelect() {
    Screen next = playerSelect.update();
    BeginDrawing();
    playerSelect.draw();
    EndDrawing();
    return next;
}

Screen Application::updateLevelSelect() {
    Screen next = levelSelect.update();
    if (next == Screen::PLAYING) {
        currentSize = levelSelect.size();
        currentSeconds = levelSelect.seconds();
        game = std::make_unique<Game>(currentSize, currentSeconds);
        generationScreen.begin(*game);
        next = Screen::GENERATING;
    }

    BeginDrawing();
    levelSelect.draw();
    EndDrawing();
    return next;
}

Screen Application::updateGenerating() {
    bool complete = generationScreen.update(*game);
    BeginDrawing();
    generationScreen.draw(*game);
    EndDrawing();
    if (complete) {
        DisableCursor();
        return Screen::PLAYING;
    }
    return Screen::GENERATING;
}

Screen Application::updatePlaying() {
    game->update(readInput());
    bool pause = IsKeyPressed(KEY_ESCAPE);
    BeginDrawing();
    renderer.draw(*game);
    EndDrawing();

    if (game->finished()) {
        EnableCursor();
        newBestTime = game->won() && players.recordWin(currentSize, game->timeTaken());
        bestTime = players.bestTime(currentSize);
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
    BeginDrawing();
    renderer.draw(*game);
    pauseMenu.draw();
    EndDrawing();

    switch (action) {
    case PauseAction::RESUME:
        DisableCursor();
        return Screen::PLAYING;
    case PauseAction::RESTART:
        game = std::make_unique<Game>(currentSize, currentSeconds);
        generationScreen.begin(*game);
        return Screen::GENERATING;
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
    BeginDrawing();
    renderer.draw(*game);
    endScreen.draw(*game, players.activeName(), bestTime, newBestTime);
    EndDrawing();

    if (next != Screen::END) {
        game.reset();
    }
    return next;
}
