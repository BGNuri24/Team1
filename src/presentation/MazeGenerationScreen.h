#pragma once
#include "../logic/Game.h"
#include "MenuBackground.h"
#include "Ui.h"

class MazeGenerationScreen {
public:
    // Initialize the preview for a fresh Game by resetting its displayed carving progress and
    // generation timing. The maze is already generated; this screen animates its recorded opening
    // sequence.
    void Begin(const Game& CurrentGame);
    // Advance the generation preview from elapsed frame time and the run's recorded steps. Return
    // whether its presentation has completed so Application can enter gameplay and capture the
    // cursor.
    bool Update(const Game& CurrentGame);
    // Render the preparation directory and current generation progress for the supplied Game.
    // This reads the chosen run and preview state without changing the maze or starting its
    // gameplay timer.
    void Draw(const Game& CurrentGame) const;

private:
    float Elapsed = 0.0f;
    int ShownSteps = 0;
    std::vector<unsigned char> Revealed;
    MenuBackground Background;
    MenuTypography Type;
};
