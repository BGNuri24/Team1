#pragma once
#include "../logic/Game.h"
#include "MenuBackground.h"
#include "Ui.h"

class MazeGenerationScreen {
public:
    void begin(const Game& game);
    bool update(const Game& game);
    void draw(const Game& game) const;

private:
    float elapsed = 0.0f;
    int shownSteps = 0;
    std::vector<unsigned char> revealed;
    MenuBackground background;
    MenuTypography type;
};
