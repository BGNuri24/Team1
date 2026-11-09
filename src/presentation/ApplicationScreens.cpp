#include "Application.h"
#include "Input.h"
#include "raylib.h"

// Wrap a drawing callback in raylib's BeginDrawing and EndDrawing pair for one frame. The
// callback supplies the screen contents while this helper owns the required frame boundaries.
template <typename Draw> static void drawFrame(Draw draw) {
    BeginDrawing();
    draw();
    EndDrawing();
}

// Update a menu once, draw its resulting state, and return the requested destination screen. The
// template shares this sequence across menus with compatible update and draw interfaces.
template <typename Menu> static Screen updateMenu(Menu& menu) {
    Screen next = menu.update();
    // Draw the updated menu inside the frame boundaries owned by drawFrame. The callback captures
    // this frame's menu by reference and is invoked immediately.
    drawFrame([&] { menu.draw(); });
    return next;
}

// Update and draw the main menu through the shared menu-frame helper. Return its requested screen
// so the application can coordinate any corridor transition.
Screen Application::updateMainMenu() {
    return updateMenu(mainMenu);
}
// Update and draw the controls page through the shared menu-frame helper. Its return value
// identifies whether the user remains on the guide or returns to another screen.
Screen Application::updateControls() {
    return updateMenu(controls);
}
// Update and draw profile selection against the application's shared Players instance. Return
// navigation requests while the menu itself handles profile actions and validation messages.
Screen Application::updatePlayerSelect() {
    return updateMenu(playerSelect);
}

// Advance the shared corridor animation while keeping normal menu input and generation timing
// frozen. Ignore the first frame's stale delta, choose the visible overlay at the midpoint, and
// publish pendingMenu only when travel ends.
Screen Application::updateMenuTravel() {
    if (travelFirstFrame)
        travelFirstFrame = false;
    else
        menuBackdrop.update(GetFrameTime());
    const Screen displayed = menuBackdrop.destinationVisible() ? pendingMenu : screen;
    // Render the corridor and its foreground together while normal page input remains frozen. The
    // journey chooses which page is visible without advancing gameplay.
    drawFrame([&] {
        menuBackdrop.draw((float)GetTime());
        // Capture only the selected page's foreground into the corridor overlay. The shared
        // backdrop remains outside that capture so its turn animation is independent.
        menuBackdrop.drawOverlay([&] { drawMenuScreen(displayed); });
    });
    return menuBackdrop.moving() ? screen : pendingMenu;
}

// Draw only the foreground for the requested menu or generation page without processing its
// input. Corridor travel uses this to change the visible overlay while navigation and generation
// remain frozen.
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

// Create a new owned Game from the selected size, timer, mode, and active palette. Reset map
// timing, prepare renderer materials, and initialize the recorded-generation preview before
// returning GENERATING.
Screen Application::startGame() {
    const MazeTheme selected = ActiveMazeTheme;
    MenuTheme::apply(selected);
    game = std::make_unique<Game>(currentSize, currentSeconds, currentMode, std::nullopt, selected);
    mapPeek.reset(currentSize, currentMode);
    renderer.prepareMazeTheme(game->theme());
    generationScreen.begin(*game);
    return Screen::GENERATING;
}

// Read level-selection input and capture confirmed size, duration, and mode before starting a
// fresh run. Draw the selection page's final frame and route confirmed play through the
// generation screen.
Screen Application::updateLevelSelect() {
    Screen next = levelSelect.update();
    if (next == Screen::PLAYING) {
        currentSize = levelSelect.size();
        currentSeconds = levelSelect.seconds();
        currentMode = levelSelect.mode();
        next = startGame();
    }

    // Draw the level-selection page's final state before the requested generation transition is
    // published. The callback reads the menu state already updated this frame.
    drawFrame([&] { levelSelect.draw(); });
    return next;
}

// Advance and draw the maze's recorded generation preview, entering gameplay only after it
// completes. Capture the cursor for active play, or enter pause immediately if the window lacks
// focus.
Screen Application::updateGenerating() {
    bool complete = generationScreen.update(*game);
    // Draw the current recorded-generation progress inside one raylib frame. This presentation
    // does not update the Game's gameplay timer.
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

// Give pause requests priority, then read sensitivity-adjusted input, update the run and map
// peek, and draw the frame. On completion, restore the cursor, save eligible wins, and prepare
// explicit record and save-failure status for results.
Screen Application::updatePlaying() {
    // Freeze immediately, including on the frame that focus is lost or Escape is pressed.
    if (gameplayPauseRequested()) {
        EnableCursor();
        pauseMenu.reset();
        // Draw the frozen world and freshly reset pause overlay on the exact pause-request frame.
        // No movement or countdown update occurs before this callback.
        drawFrame([&] {
            renderer.draw(*game);
            pauseMenu.draw();
        });
        return Screen::PAUSED;
    }
    const InputState input = readInput(settings.sensitivityMultiplier());
    game->update(input);
    mapPeek.update(input.dt, input.mapPressed);
    // Render the updated run with its current map-peek state after simulation. The callback
    // borrows both objects only for this frame.
    drawFrame([&] { renderer.draw(*game, &mapPeek); });

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

// Draw the unchanged game behind the pause menu and handle resume, retry, profile change, or
// main-menu navigation. Ignore actions while the window is inactive and release the current Game
// when leaving the run.
Screen Application::updatePaused() {
    // Returning focus leaves the run paused until the user explicitly chooses Resume.
    PauseAction action = gameplayWindowActive() ? pauseMenu.update() : PauseAction::NONE;
    // Draw the unchanged run beneath the pause menu while awaiting a permitted pause action.
    // Rendering does not resume gameplay or consume its timer.
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

// Draw the frozen final corridor and result report, then handle its navigation request. Retrying
// creates a fresh run; leaving results destroys the old run so its state cannot leak into later
// play.
Screen Application::updateEnd() {
    Screen next = endScreen.update(GetFrameTime());
    // Draw only the frozen world and results report, omitting the gameplay HUD. Pass previously
    // computed score and save-failure status without writing storage during rendering.
    drawFrame([&] {
        renderer.drawWorld(*game);
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
