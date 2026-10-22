#pragma once

#include "ControlsScreen.h"
#include "EndScreen.h"
#include "LevelSelect.h"
#include "MainMenu.h"
#include "MazeGenerationScreen.h"
#include "PauseMenu.h"
#include "PlayerSelect.h"
#include "Renderer.h"
#include "Screen.h"
#include "../logic/Game.h"
#include "../logic/Players.h"
#include <memory>

class Application {
public:
    Application();
    void run();

private:
    Screen updateCurrentScreen();
    Screen updateMainMenu();
    Screen updateControls();
    Screen updatePlayerSelect();
    Screen updateLevelSelect();
    Screen updateGenerating();
    Screen updatePlaying();
    Screen updatePaused();
    Screen updateEnd();

    Players players;
    Renderer renderer;
    MainMenu mainMenu;
    ControlsScreen controls;
    PlayerSelect playerSelect;
    LevelSelect levelSelect;
    PauseMenu pauseMenu;
    EndScreen endScreen;
    MazeGenerationScreen generationScreen;
    std::unique_ptr<Game> game;
    int currentSize;
    float currentSeconds;
    float bestTime;
    bool newBestTime;
    Screen screen;
};
