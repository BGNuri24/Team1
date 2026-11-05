#pragma once
#include "RandomSeed.h"
#include <vector>

struct MazeStep {
    int row;
    int col;
};

class MazeData {
public:
    explicit MazeData(int size, std::uint32_t seed = randomSeed());

    int size() const;
    // Return '#' for walls or out-of-bounds coordinates, '.' for passages, and 'E' for the exit.
    char cell(int r, int c) const;
    const std::vector<MazeStep>& generationSteps() const;

private:
    void carve(int r, int c, std::mt19937& random);
    int n;
    std::vector<char> grid;
    std::vector<MazeStep> steps;
};
