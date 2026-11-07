#include "Application.h"
#include "Input.h"
#include "raylib.h"

// Wrap a screen's drawing callback in the BeginDrawing/EndDrawing pair required for one raylib
// frame.
template <typename Draw> static void drawFrame(Draw draw) {
    BeginDrawing();
    draw();
    EndDrawing();
}

// Process one menu input frame, draw that menu, and return the requested destination screen.
template <typename Menu> static Screen updateMenu(Menu& menu) {
    Screen next = menu.update();
    drawFrame([&] { menu.draw(); });
    return next;
}

// Update and render the main menu through the shared menu-frame helper.
Screen Application::updateMainMenu() {
    return updateMenu(mainMenu);
}
// Update and render the controls guide through the shared menu-frame helper.
Screen Application::updateControls() {
    return updateMenu(controls);
}
// Update and render profile selection while retaining its access to the application's shared player
// store.
Screen Application::updatePlayerSelect() {
    return updateMenu(playerSelect);
}

// Keep input and generation timing frozen until the current trip finishes. The first frame drops
// the previous screen's delta, which may include texture preparation when a level was selected.
Screen Application::updateMenuTravel() {
    if (travelFirstFrame)
        travelFirstFrame = false;
    else
        menuBackdrop.update(GetFrameTime());
    const Screen displayed = menuBackdrop.destinationVisible() ? pendingMenu : screen;
    drawFrame([&] {
        menuBackdrop.draw((float)GetTime());
        menuBackdrop.drawOverlay([&] { drawMenuScreen(displayed); });
    });
    return menuBackdrop.moving() ? screen : pendingMenu;
}

void Application::drawMenuScreen(Screen requested) {
    switch (requested) {
    case Screen::MAIN_MENU:
        mainMenu.draw();
        break;
    case Screen::CONTROLS:
        controls.draw();
        break;
    case Screen::PLAYER_SELECT:
        playerSelect.draw();
        break;
    case Screen::LEVEL_SELECT:
        levelSelect.draw();
        break;
    case Screen::GENERATING:
        if (game)
            generationScreen.draw(*game);
        break;
    default:
        break;
    }
}

// Create a fresh run from the selected parameters and initialize its generation preview before
// allowing gameplay.
Screen Application::startGame() {
    const MazeTheme selected = ActiveMazeTheme;
    MenuTheme::apply(selected);
    game = std::make_unique<Game>(currentSize, currentSeconds, currentMode, std::nullopt, selected);
    renderer.prepareMazeTheme(game->theme());
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

// Advance and draw the generation preview, capture the cursor when it completes, and transition
// into gameplay.
Screen Application::updateGenerating() {
    bool complete = generationScreen.update(*game);
    drawFrame([&] { generationScreen.draw(*game); });
    if (complete) {
        if (!gameplayWindowActive()) {
            EnableCursor();
            pauseMenu.reset();
            return Screen::PAUSED;
        }
        DisableCursor();
        return Screen::PLAYING;
    }
    return Screen::GENERATING;
}

// Apply sensitivity-adjusted input and render the run, then handle completion records or open the
// pause menu.
Screen Application::updatePlaying() {
    // Freeze immediately, including on the frame that focus is lost or Escape is pressed.
    if (gameplayPauseRequested()) {
        EnableCursor();
        pauseMenu.reset();
        drawFrame([&] {
            renderer.draw(*game);
            pauseMenu.draw();
        });
        return Screen::PAUSED;
    }
    game->update(readInput(settings.sensitivityMultiplier()));
    drawFrame([&] { renderer.draw(*game); });

    if (game->finished()) {
        EnableCursor();
        endScreen.reset();
        const RecordResult result =
            game->won() ? players.recordWin(currentSize, game->timeTaken(), currentMode)
                        : RecordResult::Unchanged;
        newBestTime = result == RecordResult::Updated;
        bestTimeSaveFailed = result == RecordResult::StorageError;
        // The view uses a missing-value sentinel only after storage status has been handled
        // explicitly.
        const BestTimeResult previous = players.bestTime(currentSize, currentMode);
        bestTime = previous.seconds.value_or(-1.0f);
        return Screen::END;
    }
    return Screen::PLAYING;
}

// Render the frozen run behind the pause overlay and route its action to resume, restart, profile
// selection or the main menu.
Screen Application::updatePaused() {
    // Returning focus leaves the run paused until the user explicitly chooses Resume.
    PauseAction action = gameplayWindowActive() ? pauseMenu.update() : PauseAction::NONE;
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

// Draw the completed run and result report, releasing the run when navigation leaves the end
// screen.
Screen Application::updateEnd() {
    Screen next = endScreen.update(GetFrameTime());
    drawFrame([&] {
        renderer.draw(*game);
        endScreen.draw(*game, players.activeName(), bestTime, newBestTime, bestTimeSaveFailed);
    });

    if (next == Screen::PLAYING) {
        return startGame();
    }
    if (next != Screen::END) {
        game.reset();
    }
    return next;
}
