#include "../data/MazeData.h"
#include "../data/Validation.h"
#include <stdexcept>
#include <vector>

namespace {
void requireMaze(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Inspect the playable graph independently of the generator's candidate scoring.
int checkLayout(const MazeData& maze) {
    const int n = maze.size();
    const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
    std::vector<int> previous(n * n, -1), pending{0};
    previous[0] = 0;
    for (std::size_t head = 0; head < pending.size(); ++head) {
        const int at = pending[head];
        for (int side = 0; side < 4; ++side) {
            const int row = at / n + dr[side], col = at % n + dc[side];
            if (maze.cell(row, col) == '#')
                continue;
            const int next = row * n + col;
            if (previous[next] < 0) {
                previous[next] = at;
                pending.push_back(next);
            }
        }
    }
    requireMaze(maze.cell(0, 0) == '.' && maze.cell(n - 1, n - 1) == 'E' &&
                    previous[n * n - 1] >= 0,
                "generation must preserve the original spawn and reachable corner exit");
    int open = 0, edges = 0;
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            if (maze.cell(row, col) == '#')
                continue;
            ++open;
            requireMaze(previous[row * n + col] >= 0, "all passages must remain connected");
            edges += maze.cell(row + 1, col) != '#';
            edges += maze.cell(row, col + 1) != '#';
        }
    }
    const int roomsPerSide = (n + 1) / 2;
    requireMaze(open == 2 * roomsPerSide * roomsPerSide - 1 + (n % 2 == 0 ? 2 : 0) &&
                    edges == open - 1,
                "harder layouts must retain passage density and one unique route between cells");

    int choices = 0;
    for (int at = n * n - 1; at != 0; at = previous[at]) {
        int degree = 0;
        for (int side = 0; side < 4; ++side)
            degree += maze.cell(at / n + dr[side], at % n + dc[side]) != '#';
        choices += degree > 2;
    }
    return choices;
}

// The preview must reveal only the chosen layout, with each new cell connected to an already
// visible passage, even when carving resumes from an older branch.
void checkAnimation(const MazeData& maze) {
    const int n = maze.size();
    std::vector<bool> shown(n * n, false);
    int revealed = 0;
    for (const MazeStep& step : maze.generationSteps()) {
        requireMaze(step.row >= 0 && step.row < n && step.col >= 0 && step.col < n &&
                        maze.cell(step.row, step.col) != '#',
                    "generation animation must not reveal walls or rejected layouts");
        const int index = step.row * n + step.col;
        if (shown[index])
            continue;
        const bool connected =
            (step.row > 0 && shown[index - n]) || (step.row + 1 < n && shown[index + n]) ||
            (step.col > 0 && shown[index - 1]) || (step.col + 1 < n && shown[index + 1]);
        requireMaze((revealed == 0 && index == 0) || connected,
                    "generation animation must grow a connected network from the spawn");
        shown[index] = true;
        ++revealed;
    }
    for (int row = 0; row < n; ++row)
        for (int col = 0; col < n; ++col)
            requireMaze(shown[row * n + col] == (maze.cell(row, col) != '#'),
                        "generation preview must cover every passage in the selected layout");
    const auto& last = maze.generationSteps().back();
    requireMaze(last.row == n - 1 && last.col == n - 1,
                "generation animation must still end at the exit");
}

void checkReproduction(const MazeData& maze, unsigned seed) {
    MazeData repeated(maze.size(), seed);
    for (int row = 0; row < maze.size(); ++row)
        for (int col = 0; col < maze.size(); ++col)
            requireMaze(maze.cell(row, col) == repeated.cell(row, col),
                        "a run seed must reproduce the selected maze layout");
    const auto& steps = maze.generationSteps();
    const auto& replay = repeated.generationSteps();
    requireMaze(steps.size() == replay.size(), "a seed must reproduce generation-step count");
    for (std::size_t index = 0; index < steps.size(); ++index)
        requireMaze(steps[index].row == replay[index].row && steps[index].col == replay[index].col,
                    "a run seed must reproduce the carving animation");
}
} // namespace

// Cover every allowed dimension, including even-size exit connectors, then guard against layouts
// regressing to long corridors with very few decisions on the route to the exit.
void runMazeGenerationTests() {
    for (int size = MIN_SIZE; size <= MAX_SIZE; ++size) {
        for (unsigned seed : {0u, 1u, 1234u, 9917u}) {
            MazeData maze(size, seed);
            checkLayout(maze);
            checkAnimation(maze);
            checkReproduction(maze, seed);
        }
    }
    const int sizes[] = {15, 25, 40};
    const int minimumChoices[] = {4, 8, 14};
    for (int level = 0; level < 3; ++level) {
        int choices = 0;
        for (unsigned seed = 0; seed < 128; ++seed)
            choices += checkLayout(MazeData(sizes[level], seed));
        requireMaze(choices >= 128 * minimumChoices[level],
                    "preset mazes must offer meaningful wrong turns along the exit route");
    }
}
