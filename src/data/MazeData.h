#pragma once
#include "RandomSeed.h"
#include <vector>

struct MazeStep {
    int row;
    int col;
};

class MazeData {
public:
    // Generate a square maze from the supplied size and random seed, retaining the strongest of
    // six scored layouts and its matching carving history. Callers supply a supported size;
    // identical size and seed reproduce the layout.
    explicit MazeData(int size, std::uint32_t seed = randomSeed());

    // Return the number of rows and columns in this square maze. This reads the stored dimension
    // without generating cells or copying the grid.
    int size() const;
    // Read a cell using row-major indexing: row * size + column. Return '#' for walls and
    // out-of-bounds positions, '.' for passages, and 'E' for the exit, allowing callers to treat
    // the maze boundary as solid.
    char cell(int r, int c) const;
    // Borrow the recorded cell-opening sequence for the selected layout so the loading screen can
    // animate generation without regenerating the maze. The reference remains valid only while
    // this MazeData object exists.
    const std::vector<MazeStep>& generationSteps() const;

private:
    // Carve a connected layout using a growing collection of frontier cells and the supplied
    // generator. Move two cells at a time, opening the intervening wall and recording both
    // openings; occasional older-frontier choices create more branches.
    void carve(std::mt19937& random);
    int n;
    std::vector<char> grid;
    std::vector<MazeStep> steps;
};
