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
    Application();
    void run();

private:
    Screen updateCurrentScreen();
    Screen updateMenuTravel();
    void drawMenuScreen(Screen requested);
    Screen updateMainMenu();
    Screen updateControls();
    Screen updatePlayerSelect();
    Screen updateLevelSelect();
    Screen updateGenerating();
    Screen updatePlaying();
    Screen updatePaused();
    Screen updateEnd();
    Screen startGame();
    void prepareTheme();

    Players players;
    GameSettings settings;
    MenuBackground menuBackdrop;
    Renderer renderer;
    MainMenu mainMenu;
    ControlsScreen controls;
    PlayerSelect playerSelect;
    LevelSelect levelSelect;
    PauseMenu pauseMenu;
    EndScreen endScreen;
    MazeGenerationScreen generationScreen;
    std::unique_ptr<Game> game;
    MapPeek mapPeek;
    int currentSize;
    float currentSeconds;
    GameMode currentMode;
    float bestTime;
    bool newBestTime;
    bool bestTimeSaveFailed = false;
    Screen screen;
    Screen pendingMenu = Screen::MAIN_MENU;
    bool travelFirstFrame = false;
};
