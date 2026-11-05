#include "MazeData.h"
#include <algorithm>
#include <utility>

// Initialize a wall-filled square grid, carve connected passages, and record the generation steps
// ending at the exit.
MazeData::MazeData(int size, std::uint32_t seed) : n(size), grid(size* size, '#') {
    std::mt19937 random(seed);
    // Carve the connected passage network from the top-left starting cell.
    carve(0, 0, random);

    // For even dimensions, connect the carved grid to the bottom-right corner,
    // which lies outside the generator's two-cell stepping pattern.
    if (n % 2 == 0) {
        grid[(n - 2) * n + n - 1] = '.';
        steps.push_back({ n - 2, n - 1 });
    }

    // Mark the bottom-right cell as the exit and include it in the generation animation.
    grid[(n - 1) * n + n - 1] = 'E';
    steps.push_back({ n - 1, n - 1 });
}

// Use randomized depth-first traversal in two-cell increments, opening each connecting wall and
// recording newly carved cells.
void MazeData::carve(int r, int c, std::mt19937& random) {
    grid[r * n + c] = '.';
    steps.push_back({ r, c });
    int dr[4] = { -2, 2, 0, 0 };
    int dc[4] = { 0, 0, -2, 2 };
    int o[4] = { 0, 1, 2, 3 };
    std::shuffle(std::begin(o), std::end(o), random);

    for (int k = 0; k < 4; k++) {
        int nr = r + dr[o[k]], nc = c + dc[o[k]];
        if (nr >= 0 && nr < n && nc >= 0 && nc < n && grid[nr * n + nc] == '#') {
            int wallR = r + dr[o[k]] / 2;
            int wallC = c + dc[o[k]] / 2;
            grid[wallR * n + wallC] = '.';
            steps.push_back({ wallR, wallC });
            carve(nr, nc, random);
        }
    }
}

// Return the number of rows and columns in the square maze.
int MazeData::size() const {
    return n;
}
// Expose the recorded carving order by reference so the generation screen can animate it without
// copying the steps.
const std::vector<MazeStep>& MazeData::generationSteps() const {
    return steps;
}

// Look up a row/column cell and treat coordinates outside the grid as solid walls.
char MazeData::cell(int r, int c) const {
    if (r < 0 || r >= n || c < 0 || c >= n)
        return '#';
    return grid[r * n + c];
}
