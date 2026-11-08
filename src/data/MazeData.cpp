#include "MazeData.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace {
// Prefer decision points and turns along the actual exit route. Penalize excessive detours so
// making navigation harder does not just consume the existing level timers with longer walks.
int routeDifficulty(const MazeData& maze) {
    const int n = maze.size();
    const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
    std::vector<int> previous(n * n, -1), pending{0};
    previous[0] = 0;
    for (std::size_t head = 0; head < pending.size(); ++head) {
        const int cell = pending[head];
        for (int direction = 0; direction < 4; ++direction) {
            const int row = cell / n + dr[direction], col = cell % n + dc[direction];
            if (maze.cell(row, col) == '#')
                continue;
            const int next = row * n + col;
            if (previous[next] < 0) {
                previous[next] = cell;
                pending.push_back(next);
            }
        }
    }

    int length = 0, turns = 0, choices = 0, lastDirection = 0;
    for (int cell = n * n - 1; cell != 0; cell = previous[cell]) {
        const int direction = cell - previous[cell];
        turns += lastDirection != 0 && direction != lastDirection;
        lastDirection = direction;
        ++length;
        int neighbors = 0;
        for (int side = 0; side < 4; ++side)
            neighbors += maze.cell(cell / n + dr[side], cell % n + dc[side]) != '#';
        choices += neighbors > 2;
    }
    if (length > 6 * (n - 1))
        return -length;
    return choices * 12 + turns + length / 4;
}
} // namespace

// Try a small, fixed number of seeded layouts and retain the more challenging exit route together
// with its own carving animation. All candidates keep the same dimensions, spawn and exit.
MazeData::MazeData(int size, std::uint32_t seed) : n(size), grid(size * size, '#') {
    std::mt19937 random(seed);
    int bestScore = std::numeric_limits<int>::min();
    std::vector<char> bestGrid;
    std::vector<MazeStep> bestSteps;
    for (int attempt = 0; attempt < 6; ++attempt) {
        std::fill(grid.begin(), grid.end(), '#');
        steps.clear();
        carve(random);

        // For even dimensions, join the two-cell carving pattern to the bottom-right exit.
        if (n % 2 == 0) {
            grid[(n - 2) * n + n - 1] = '.';
            steps.push_back({n - 2, n - 1});
        }
        grid[n * n - 1] = 'E';
        steps.push_back({n - 1, n - 1});

        const int score = routeDifficulty(*this);
        if (score > bestScore) {
            bestScore = score;
            bestGrid = grid;
            bestSteps = steps;
        }
    }
    grid = std::move(bestGrid);
    steps = std::move(bestSteps);
}

// Grow mostly from the newest passage, occasionally returning to an older frontier to form more
// branches. Only connect unvisited cells, keeping every passage reachable without creating loops.
void MazeData::carve(std::mt19937& random) {
    const int dr[] = {-2, 2, 0, 0}, dc[] = {0, 0, -2, 2};
    std::vector<int> active{0};
    grid[0] = '.';
    steps.push_back({0, 0});
    while (!active.empty()) {
        const int newest = (int)active.size() - 1;
        const int index = std::uniform_int_distribution<int>(0, 15)(random) == 0
                              ? std::uniform_int_distribution<int>(0, newest)(random)
                              : newest;
        const int row = active[index] / n, col = active[index] % n;
        int order[] = {0, 1, 2, 3};
        std::shuffle(std::begin(order), std::end(order), random);
        bool carved = false;
        for (int direction : order) {
            const int nextRow = row + dr[direction], nextCol = col + dc[direction];
            if (nextRow < 0 || nextRow >= n || nextCol < 0 || nextCol >= n ||
                grid[nextRow * n + nextCol] != '#')
                continue;
            const int wallRow = row + dr[direction] / 2;
            const int wallCol = col + dc[direction] / 2;
            grid[wallRow * n + wallCol] = '.';
            steps.push_back({wallRow, wallCol});
            grid[nextRow * n + nextCol] = '.';
            steps.push_back({nextRow, nextCol});
            active.push_back(nextRow * n + nextCol);
            carved = true;
            break;
        }
        if (!carved)
            active.erase(active.begin() + index);
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
