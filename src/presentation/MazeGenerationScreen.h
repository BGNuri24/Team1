#pragma once
#include "../logic/Game.h"

class MazeGenerationScreen {
public:
    void begin(const Game& game);
    bool update(const Game& game);
    void draw(const Game& game) const;
private:
    float elapsed = 0.0f;
    int shownSteps = 0;
    std::vector<unsigned char> revealed;
};
