#pragma once

#include "../data/MazeData.h"
#include <vector>

class MenuBackground {
public:
    MenuBackground();
    void draw(float time, float horizontalOffset = 0.24f) const;

private:
    MazeData maze;
    std::vector<MazeStep> route;
};
