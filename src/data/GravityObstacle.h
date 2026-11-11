#pragma once

// Describe a barrier spanning a straight passage, independently of its rendered decoration.
struct GravityObstacle {
    int Row;
    int Col;
    bool AcrossX;
    bool Ceiling;
};
