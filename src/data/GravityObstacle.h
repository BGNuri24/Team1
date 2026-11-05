#pragma once

// Describe a barrier spanning a straight passage, independently of its rendered decoration.
struct GravityObstacle {
    int row;
    int col;
    bool acrossX;
    bool ceiling;
};
