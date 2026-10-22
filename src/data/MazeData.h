#pragma once
#include <vector>

struct MazeStep {
    int row;
    int col;
};

class MazeData {
public:
    explicit MazeData(int size);

    int size() const;
    // '#' wall, '.' path, 'E' exit
    char cell(int r, int c) const;
    const std::vector<MazeStep>& generationSteps() const;
private:
    void carve(int r, int c);
    int n;
    std::vector<char> grid;
    std::vector<MazeStep> steps;
};