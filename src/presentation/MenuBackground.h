#pragma once

#include "../data/MazeData.h"
#include <vector>

class MenuBackground {
public:
    MenuBackground();
    void draw(float time) const;

private:
    MazeData maze;
    std::vector<MazeStep> route;
};
