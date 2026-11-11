#include "MazeData.h"
#include <algorithm>
#include <limits>
#include <utility>

namespace {
// Find the exit route with breadth-first search and score its branches, turns, and length.
// Penalize routes longer than six times the side-length distance so difficulty does not come
// mainly from consuming the level timer.
int RouteDifficulty(const MazeData& Maze) {
    const int N = Maze.Size();
    const int Dr[] = {-1, 1, 0, 0}, Dc[] = {0, 0, -1, 1};
    std::vector<int> Previous(N * N, -1), Pending{0};
    Previous[0] = 0;
    for (std::size_t Head = 0; Head < Pending.size(); ++Head) {
        const int CurrentCell = Pending[Head];
        for (int Direction = 0; Direction < 4; ++Direction) {
            const int Row = CurrentCell / N + Dr[Direction], Col = CurrentCell % N + Dc[Direction];
            if (Maze.Cell(Row, Col) == '#')
                continue;
            const int Next = Row * N + Col;
            if (Previous[Next] < 0) {
                Previous[Next] = CurrentCell;
                Pending.push_back(Next);
            }
        }
    }

    int Length = 0, Turns = 0, Choices = 0, LastDirection = 0;
    for (int CurrentCell = N * N - 1; CurrentCell != 0; CurrentCell = Previous[CurrentCell]) {
        const int Direction = CurrentCell - Previous[CurrentCell];
        Turns += LastDirection != 0 && Direction != LastDirection;
        LastDirection = Direction;
        ++Length;
        int Neighbors = 0;
        for (int CurrentSide = 0; CurrentSide < 4; ++CurrentSide)
            Neighbors += Maze.Cell(CurrentCell / N + Dr[CurrentSide], CurrentCell % N + Dc[CurrentSide]) != '#';
        Choices += Neighbors > 2;
    }
    if (Length > 6 * (N - 1))
        return -Length;
    return Choices * 12 + Turns + Length / 4;
}
} // namespace

// Generate a square maze from the supplied size and random seed, retaining the strongest of six
// scored layouts and its matching carving history. Callers supply a supported size; identical
// size and seed reproduce the layout.
MazeData::MazeData(int CurrentSize, std::uint32_t CurrentSeed) : N(CurrentSize), Grid(CurrentSize * CurrentSize, '#') {
    std::mt19937 Random(CurrentSeed);
    int BestScore = std::numeric_limits<int>::min();
    std::vector<char> BestGrid;
    std::vector<MazeStep> BestSteps;
    for (int Attempt = 0; Attempt < 6; ++Attempt) {
        std::fill(Grid.begin(), Grid.end(), '#');
        Steps.clear();
        Carve(Random);

        // For even dimensions, join the two-cell carving pattern to the bottom-right exit.
        if (N % 2 == 0) {
            Grid[(N - 2) * N + N - 1] = '.';
            Steps.push_back({N - 2, N - 1});
        }
        Grid[N * N - 1] = 'E';
        Steps.push_back({N - 1, N - 1});

        const int CurrentScore = RouteDifficulty(*this);
        if (CurrentScore > BestScore) {
            BestScore = CurrentScore;
            BestGrid = Grid;
            BestSteps = Steps;
        }
    }
    Grid = std::move(BestGrid);
    Steps = std::move(BestSteps);
}

// Carve a connected layout using a growing collection of frontier cells and the supplied
// generator. Move two cells at a time, opening the intervening wall and recording both openings;
// occasional older-frontier choices create more branches.
void MazeData::Carve(std::mt19937& Random) {
    const int Dr[] = {-2, 2, 0, 0}, Dc[] = {0, 0, -2, 2};
    std::vector<int> Active{0};
    Grid[0] = '.';
    Steps.push_back({0, 0});
    while (!Active.empty()) {
        const int Newest = (int)Active.size() - 1;
        const int Index = std::uniform_int_distribution<int>(0, 15)(Random) == 0
                              ? std::uniform_int_distribution<int>(0, Newest)(Random)
                              : Newest;
        const int Row = Active[Index] / N, Col = Active[Index] % N;
        int Order[] = {0, 1, 2, 3};
        std::shuffle(std::begin(Order), std::end(Order), Random);
        bool Carved = false;
        for (int Direction : Order) {
            const int NextRow = Row + Dr[Direction], NextCol = Col + Dc[Direction];
            if (NextRow < 0 || NextRow >= N || NextCol < 0 || NextCol >= N ||
                Grid[NextRow * N + NextCol] != '#')
                continue;
            const int WallRow = Row + Dr[Direction] / 2;
            const int WallCol = Col + Dc[Direction] / 2;
            Grid[WallRow * N + WallCol] = '.';
            Steps.push_back({WallRow, WallCol});
            Grid[NextRow * N + NextCol] = '.';
            Steps.push_back({NextRow, NextCol});
            Active.push_back(NextRow * N + NextCol);
            Carved = true;
            break;
        }
        if (!Carved)
            Active.erase(Active.begin() + Index);
    }
}

// Return the number of rows and columns in this square maze. This reads the stored dimension
// without generating cells or copying the grid.
int MazeData::Size() const {
    return N;
}
// Borrow the recorded cell-opening sequence for the selected layout so the loading screen can
// animate generation without regenerating the maze. The reference remains valid only while this
// MazeData object exists.
const std::vector<MazeStep>& MazeData::GenerationSteps() const {
    return Steps;
}

// Read a cell using row-major indexing: row * size + column. Return '#' for walls and
// out-of-bounds positions, '.' for passages, and 'E' for the exit, allowing callers to treat the
// maze boundary as solid.
char MazeData::Cell(int CurrentR, int CurrentC) const {
    if (CurrentR < 0 || CurrentR >= N || CurrentC < 0 || CurrentC >= N)
        return '#';
    return Grid[CurrentR * N + CurrentC];
}
