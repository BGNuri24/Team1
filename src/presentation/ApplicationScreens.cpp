#include "Application.h"
#include "Input.h"
#include "raylib.h"

// Wrap a drawing callback in raylib's BeginDrawing and EndDrawing pair for one frame. The
// callback supplies the screen contents while this helper owns the required frame boundaries.
template <typename Draw> static void DrawFrame(Draw CurrentDraw) {
    BeginDrawing();
    CurrentDraw();
    EndDrawing();
}

// Update a menu once, draw its resulting state, and return the requested destination screen. The
// template shares this sequence across menus with compatible update and draw interfaces.
template <typename Menu> static Screen UpdateMenu(Menu& CurrentMenu) {
    Screen Next = CurrentMenu.Update();
    // Draw the updated menu inside the frame boundaries owned by drawFrame. The callback captures
    // this frame's menu by reference and is invoked immediately.
    DrawFrame([&] { CurrentMenu.Draw(); });
    return Next;
}

// Update and draw the main menu through the shared menu-frame helper. Return its requested screen
// so the application can coordinate any corridor transition.
Screen Application::UpdateMainMenu() {
    return UpdateMenu(CurrentMainMenu);
}
// Update and draw the controls page through the shared menu-frame helper. Its return value
// identifies whether the user remains on the guide or returns to another screen.
Screen Application::UpdateControls() {
    return UpdateMenu(CurrentControls);
}
// Update and draw profile selection against the application's shared Players instance. Return
// navigation requests while the menu itself handles profile actions and validation messages.
Screen Application::UpdatePlayerSelect() {
    return UpdateMenu(CurrentPlayerSelect);
}

// Advance the shared corridor animation while keeping normal menu input and generation timing
// frozen. Ignore the first frame's stale delta, choose the visible overlay at the midpoint, and
// publish pendingMenu only when travel ends.
Screen Application::UpdateMenuTravel() {
    if (TravelFirstFrame)
        TravelFirstFrame = false;
    else
        MenuBackdrop.Update(GetFrameTime());
    const Screen Displayed = MenuBackdrop.DestinationVisible() ? PendingMenu : CurrentScreen;
    // Render the corridor and its foreground together while normal page input remains frozen. The
    // journey chooses which page is visible without advancing gameplay.
    DrawFrame([&] {
        MenuBackdrop.Draw((float)GetTime());
        // Capture only the selected page's foreground into the corridor overlay. The shared
        // backdrop remains outside that capture so its turn animation is independent.
        MenuBackdrop.DrawOverlay([&] { DrawMenuScreen(Displayed); });
    });
    return MenuBackdrop.Moving() ? CurrentScreen : PendingMenu;
}

// Draw only the foreground for the requested menu or generation page without processing its
// input. Corridor travel uses this to change the visible overlay while navigation and generation
// remain frozen.
void Application::DrawMenuScreen(Screen Requested) {
    switch (Requested) {
    case Screen::MainMenu:
        CurrentMainMenu.Draw();
        break;
    case Screen::CONTROLS:
        CurrentControls.Draw();
        break;
    case Screen::PlayerSelect:
        CurrentPlayerSelect.Draw();
        break;
    case Screen::LevelSelect:
        CurrentLevelSelect.Draw();
        break;
    case Screen::GENERATING:
        if (CurrentGame)
            GenerationScreen.Draw(*CurrentGame);
        break;
    default:
        break;
    }
}

// Create a new owned Game from the selected size, timer, mode, and active palette. Reset map
// timing, prepare renderer materials, and initialize the recorded-generation preview before
// returning GENERATING.
Screen Application::StartGame() {
    const MazeTheme CurrentSelected = ActiveMazeTheme;
    MenuTheme::Apply(CurrentSelected);
    CurrentGame = std::make_unique<Game>(CurrentSize, CurrentSeconds, CurrentMode, std::nullopt, CurrentSelected);
    CurrentMapPeek.Reset(CurrentSize, CurrentMode);
    CurrentRenderer.PrepareMazeTheme(CurrentGame->Theme());
    GenerationScreen.Begin(*CurrentGame);
    return Screen::GENERATING;
}

// Read level-selection input and capture confirmed size, duration, and mode before starting a
// fresh run. Draw the selection page's final frame and route confirmed play through the
// generation screen.
Screen Application::UpdateLevelSelect() {
    Screen Next = CurrentLevelSelect.Update();
    if (Next == Screen::PLAYING) {
        CurrentSize = CurrentLevelSelect.Size();
        CurrentSeconds = CurrentLevelSelect.Seconds();
        CurrentMode = CurrentLevelSelect.Mode();
        Next = StartGame();
    }

    // Draw the level-selection page's final state before the requested generation transition is
    // published. The callback reads the menu state already updated this frame.
    DrawFrame([&] { CurrentLevelSelect.Draw(); });
    return Next;
}

// Advance and draw the maze's recorded generation preview, entering gameplay only after it
// completes. Capture the cursor for active play, or enter pause immediately if the window lacks
// focus.
Screen Application::UpdateGenerating() {
    bool Complete = GenerationScreen.Update(*CurrentGame);
    // Draw the current recorded-generation progress inside one raylib frame. This presentation
    // does not update the Game's gameplay timer.
    DrawFrame([&] { GenerationScreen.Draw(*CurrentGame); });
    if (Complete) {
        if (!GameplayWindowActive()) {
            EnableCursor();
            CurrentPauseMenu.Reset();
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
Screen Application::UpdatePlaying() {
    // Freeze immediately, including on the frame that focus is lost or Escape is pressed.
    if (GameplayPauseRequested()) {
        EnableCursor();
        CurrentPauseMenu.Reset();
        // Draw the frozen world and freshly reset pause overlay on the exact pause-request frame.
        // No movement or countdown update occurs before this callback.
        DrawFrame([&] {
            CurrentRenderer.Draw(*CurrentGame);
            CurrentPauseMenu.Draw();
        });
        return Screen::PAUSED;
    }
    const InputState CurrentInput = ReadInput(CurrentSettings.SensitivityMultiplier());
    CurrentGame->Update(CurrentInput);
    CurrentMapPeek.Update(CurrentInput.Dt, CurrentInput.MapPressed);
    // Render the updated run with its current map-peek state after simulation. The callback
    // borrows both objects only for this frame.
    DrawFrame([&] { CurrentRenderer.Draw(*CurrentGame, &CurrentMapPeek); });

    if (CurrentGame->Finished()) {
        EnableCursor();
        CurrentEndScreen.Reset();
        const RecordResult Result =
            CurrentGame->Won() ? CurrentPlayers.RecordWin(CurrentSize, CurrentGame->TimeTaken(), CurrentMode)
                        : RecordResult::Unchanged;
        NewBestTime = Result == RecordResult::Updated;
        BestTimeSaveFailed = Result == RecordResult::StorageError;
        // The view uses a missing-value sentinel only after storage status has been handled
        // explicitly.
        const BestTimeResult Previous = CurrentPlayers.BestTime(CurrentSize, CurrentMode);
        BestTime = Previous.Seconds.value_or(-1.0f);
        return Screen::END;
    }
    return Screen::PLAYING;
}

// Draw the unchanged game behind the pause menu and handle resume, retry, profile change, or
// main-menu navigation. Ignore actions while the window is inactive and release the current Game
// when leaving the run.
Screen Application::UpdatePaused() {
    // Returning focus leaves the run paused until the user explicitly chooses Resume.
    PauseAction Action = GameplayWindowActive() ? CurrentPauseMenu.Update() : PauseAction::NONE;
    // Draw the unchanged run beneath the pause menu while awaiting a permitted pause action.
    // Rendering does not resume gameplay or consume its timer.
    DrawFrame([&] {
        CurrentRenderer.Draw(*CurrentGame);
        CurrentPauseMenu.Draw();
    });

    switch (Action) {
    case PauseAction::RESUME:
        DisableCursor();
        return Screen::PLAYING;
    case PauseAction::RESTART:
        return StartGame();
    case PauseAction::ChangePlayer:
        CurrentGame.reset();
        return Screen::PlayerSelect;
    case PauseAction::MainMenu:
        CurrentGame.reset();
        return Screen::MainMenu;
    case PauseAction::NONE:
        return Screen::PAUSED;
    }
    return Screen::PAUSED;
}

// Draw the frozen final corridor and result report, then handle its navigation request. Retrying
// creates a fresh run; leaving results destroys the old run so its state cannot leak into later
// play.
Screen Application::UpdateEnd() {
    Screen Next = CurrentEndScreen.Update(GetFrameTime());
    // Draw only the frozen world and results report, omitting the gameplay HUD. Pass previously
    // computed score and save-failure status without writing storage during rendering.
    DrawFrame([&] {
        CurrentRenderer.DrawWorld(*CurrentGame);
        CurrentEndScreen.Draw(*CurrentGame, CurrentPlayers.ActiveName(), BestTime, NewBestTime, BestTimeSaveFailed);
    });

    if (Next == Screen::PLAYING) {
        return StartGame();
    }
    if (Next != Screen::END) {
        CurrentGame.reset();
    }
    return Next;
}
