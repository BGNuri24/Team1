#pragma once
#include "RandomSeed.h"
#include <vector>

struct MazeStep {
    int Row;
    int Col;
};

class MazeData {
public:
    // Generate a square maze from the supplied size and random seed, retaining the strongest of
    // six scored layouts and its matching carving history. Callers supply a supported size;
    // identical size and seed reproduce the layout.
    explicit MazeData(int CurrentSize, std::uint32_t CurrentSeed = RandomSeed());

    // Return the number of rows and columns in this square maze. This reads the stored dimension
    // without generating cells or copying the grid.
    int Size() const;
    // Read a cell using row-major indexing: row * size + column. Return '#' for walls and
    // out-of-bounds positions, '.' for passages, and 'E' for the exit, allowing callers to treat
    // the maze boundary as solid.
    char Cell(int CurrentR, int CurrentC) const;
    // Borrow the recorded cell-opening sequence for the selected layout so the loading screen can
    // animate generation without regenerating the maze. The reference remains valid only while
    // this MazeData object exists.
    const std::vector<MazeStep>& GenerationSteps() const;

private:
    // Carve a connected layout using a growing collection of frontier cells and the supplied
    // generator. Move two cells at a time, opening the intervening wall and recording both
    // openings; occasional older-frontier choices create more branches.
    void Carve(std::mt19937& Random);
    int N;
    std::vector<char> Grid;
    std::vector<MazeStep> Steps;
};
