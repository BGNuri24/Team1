#pragma once
#include "../logic/GameSettings.h"
#include "Ui.h"

enum class PauseAction { NONE, RESUME, RESTART, ChangePlayer, MainMenu };

class PauseMenu {
public:
    // Borrow the application's live GameSettings object and initialize the selection to Resume.
    // The menu does not own settings and applies adjustments through their validated setter.
    explicit PauseMenu(GameSettings& CurrentSettings);
    // Handle action navigation, pointer controls, slider dragging, and repeated sensitivity
    // adjustments. Save at interaction boundaries and return a PauseAction for Application to
    // perform.
    PauseAction Update();
    // Draw the dark hotel pause overlay, action selection, and opaque sensitivity controls over
    // the frozen world. Show persistence feedback from update without saving or changing gameplay
    // during rendering.
    void Draw() const;
    // Select Resume and cancel any in-progress slider drag when the pause screen opens. Preserve
    // the live sensitivity and persistence state rather than resetting the player's preference.
    void Reset();

private:
    // Ask GameSettings to save any pending preference and retain whether the attempt failed. The
    // drawing path uses that outcome for inline feedback without falsely reporting success.
    void PersistSensitivity();
    GameSettings& CurrentSettings;
    int CurrentSelected;
    bool Dragging = false;
    bool SaveFailed = false;
    MenuTypography Type;
};
