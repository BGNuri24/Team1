#include "Application.h"
#include "Input.h"
#include "raylib.h"

// Wrap a screen's drawing callback in the BeginDrawing/EndDrawing pair required for one raylib frame.
template <typename Draw>
static void drawFrame(Draw draw) {
    BeginDrawing();
    draw();
    EndDrawing();
}

// Process one menu input frame, draw that menu, and return the requested destination screen.
template <typename Menu>
static Screen updateMenu(Menu& menu) {
    Screen next = menu.update();
    drawFrame([&] { menu.draw(); });
    return next;
}

// Update and render the main menu through the shared menu-frame helper.
Screen Application::updateMainMenu() { return updateMenu(mainMenu); }
// Update and render the controls guide through the shared menu-frame helper.
Screen Application::updateControls() { return updateMenu(controls); }
// Update and render profile selection while retaining its access to the application's shared player
// store.
Screen Application::updatePlayerSelect() { return updateMenu(playerSelect); }

// Create a fresh run from the selected parameters and initialize its generation preview before
// allowing gameplay.
Screen Application::startGame() {
    game = std::make_unique<Game>(currentSize, currentSeconds, currentMode);
    generationScreen.begin(*game);
    return Screen::GENERATING;
}

// Capture a confirmed size, duration and mode, then start the generation preview while drawing the
// selection screen's final frame.
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

// Advance and draw the generation preview, capture the cursor when it completes, and transition into
// gameplay.
Screen Application::updateGenerating() {
    bool complete = generationScreen.update(*game);
    drawFrame([&] { generationScreen.draw(*game); });
    if (complete) {
        DisableCursor();
        return Screen::PLAYING;
    }
    return Screen::GENERATING;
}

// Apply sensitivity-adjusted input and render the run, then handle completion records or open the
// pause menu.
Screen Application::updatePlaying() {
    game->update(readInput(settings.sensitivityMultiplier()));
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

// Render the frozen run behind the pause overlay and route its action to resume, restart, profile
// selection or the main menu.
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

// Draw the completed run and result report, releasing the run when navigation leaves the end screen.
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
