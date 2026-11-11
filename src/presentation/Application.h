#pragma once

#include "../logic/Game.h"
#include "../logic/GameSettings.h"
#include "../logic/MapPeek.h"
#include "../logic/Players.h"
#include "ControlsScreen.h"
#include "EndScreen.h"
#include "LevelSelect.h"
#include "MainMenu.h"
#include "MazeGenerationScreen.h"
#include "PauseMenu.h"
#include "PlayerSelect.h"
#include "Renderer.h"
#include "Screen.h"
#include <memory>

class Application {
public:
    // Connect profile selection to shared players and pause controls to shared settings, then
    // initialize run parameters and the main-menu state. Prepare the active palette and place the
    // shared corridor at its entrance.
    Application();
    // Dispatch frames until a screen requests Quit or the operating system requests window
    // closure. Each handler returns the next application state and performs that screen's update
    // and drawing.
    void Run();

private:
    // Dispatch one frame to the active state, giving an in-progress corridor journey priority
    // over normal screen input. Route menu changes through travel animation and snap returns from
    // gameplay to the correct menu stop.
    Screen UpdateCurrentScreen();
    // Advance the shared corridor animation while keeping normal menu input and generation timing
    // frozen. Ignore the first frame's stale delta, choose the visible overlay at the midpoint,
    // and publish pendingMenu only when travel ends.
    Screen UpdateMenuTravel();
    // Draw only the foreground for the requested menu or generation page without processing its
    // input. Corridor travel uses this to change the visible overlay while navigation and
    // generation remain frozen.
    void DrawMenuScreen(Screen Requested);
    // Update and draw the main menu through the shared menu-frame helper. Return its requested
    // screen so the application can coordinate any corridor transition.
    Screen UpdateMainMenu();
    // Update and draw the controls page through the shared menu-frame helper. Its return value
    // identifies whether the user remains on the guide or returns to another screen.
    Screen UpdateControls();
    // Update and draw profile selection against the application's shared Players instance. Return
    // navigation requests while the menu itself handles profile actions and validation messages.
    Screen UpdatePlayerSelect();
    // Read level-selection input and capture confirmed size, duration, and mode before starting a
    // fresh run. Draw the selection page's final frame and route confirmed play through the
    // generation screen.
    Screen UpdateLevelSelect();
    // Advance and draw the maze's recorded generation preview, entering gameplay only after it
    // completes. Capture the cursor for active play, or enter pause immediately if the window
    // lacks focus.
    Screen UpdateGenerating();
    // Give pause requests priority, then read sensitivity-adjusted input, update the run and map
    // peek, and draw the frame. On completion, restore the cursor, save eligible wins, and
    // prepare explicit record and save-failure status for results.
    Screen UpdatePlaying();
    // Draw the unchanged game behind the pause menu and handle resume, retry, profile change, or
    // main-menu navigation. Ignore actions while the window is inactive and release the current
    // Game when leaving the run.
    Screen UpdatePaused();
    // Draw the frozen final corridor and result report, then handle its navigation request.
    // Retrying creates a fresh run; leaving results destroys the old run so its state cannot leak
    // into later play.
    Screen UpdateEnd();
    // Create a new owned Game from the selected size, timer, mode, and active palette. Reset map
    // timing, prepare renderer materials, and initialize the recorded-generation preview before
    // returning GENERATING.
    Screen StartGame();
    // Keep the menu palette synchronized with ActiveMazeTheme, applying it only when the selected
    // theme differs. This avoids unnecessary theme changes while menus are repeatedly updated.
    void PrepareTheme();

    Players CurrentPlayers;
    GameSettings CurrentSettings;
    MenuBackground MenuBackdrop;
    Renderer CurrentRenderer;
    MainMenu CurrentMainMenu;
    ControlsScreen CurrentControls;
    PlayerSelect CurrentPlayerSelect;
    LevelSelect CurrentLevelSelect;
    PauseMenu CurrentPauseMenu;
    EndScreen CurrentEndScreen;
    MazeGenerationScreen GenerationScreen;
    std::unique_ptr<Game> CurrentGame;
    MapPeek CurrentMapPeek;
    int CurrentSize;
    float CurrentSeconds;
    GameMode CurrentMode;
    float BestTime;
    bool NewBestTime;
    bool BestTimeSaveFailed = false;
    Screen CurrentScreen;
    Screen PendingMenu = Screen::MainMenu;
    bool TravelFirstFrame = false;
};
