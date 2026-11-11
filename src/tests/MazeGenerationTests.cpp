#include "../data/MazeData.h"
#include "../data/Validation.h"
#include <stdexcept>
#include <vector>

namespace {
// Throw the supplied explanation when a generated layout or preview violates an invariant. The
// core suite catches this exception and reports which generation property regressed.
void RequireMaze(bool Condition, const char* Message) {
    if (!Condition)
        throw std::runtime_error(Message);
}

// Inspect passage connectivity, graph structure, exit reachability, and route decisions
// independently of the generator's scoring helper. Return the measured route characteristic for
// broader difficulty checks.
int CheckLayout(const MazeData& Maze) {
    const int N = Maze.Size();
    const int Dr[] = {-1, 1, 0, 0}, Dc[] = {0, 0, -1, 1};
    std::vector<int> Previous(N * N, -1), Pending{0};
    Previous[0] = 0;
    for (std::size_t Head = 0; Head < Pending.size(); ++Head) {
        const int At = Pending[Head];
        for (int CurrentSide = 0; CurrentSide < 4; ++CurrentSide) {
            const int Row = At / N + Dr[CurrentSide], Col = At % N + Dc[CurrentSide];
            if (Maze.Cell(Row, Col) == '#')
                continue;
            const int Next = Row * N + Col;
            if (Previous[Next] < 0) {
                Previous[Next] = At;
                Pending.push_back(Next);
            }
        }
    }
    RequireMaze(Maze.Cell(0, 0) == '.' && Maze.Cell(N - 1, N - 1) == 'E' &&
                    Previous[N * N - 1] >= 0,
                "generation must preserve the original spawn and reachable corner exit");
    int CurrentOpen = 0, Edges = 0;
    for (int Row = 0; Row < N; ++Row) {
        for (int Col = 0; Col < N; ++Col) {
            if (Maze.Cell(Row, Col) == '#')
                continue;
            ++CurrentOpen;
            RequireMaze(Previous[Row * N + Col] >= 0, "all passages must remain connected");
            Edges += Maze.Cell(Row + 1, Col) != '#';
            Edges += Maze.Cell(Row, Col + 1) != '#';
        }
    }
    const int RoomsPerSide = (N + 1) / 2;
    RequireMaze(CurrentOpen == 2 * RoomsPerSide * RoomsPerSide - 1 + (N % 2 == 0 ? 2 : 0) &&
                    Edges == CurrentOpen - 1,
                "harder layouts must retain passage density and one unique route between cells");

    int Choices = 0;
    for (int At = N * N - 1; At != 0; At = Previous[At]) {
        int Degree = 0;
        for (int CurrentSide = 0; CurrentSide < 4; ++CurrentSide)
            Degree += Maze.Cell(At / N + Dr[CurrentSide], At % N + Dc[CurrentSide]) != '#';
        Choices += Degree > 2;
    }
    return Choices;
}

// Replay recorded carving steps and require each new passage to belong to the selected layout and
// connect to an already visible passage. This catches mismatched candidate histories and
// disconnected preview jumps.
void CheckAnimation(const MazeData& Maze) {
    const int N = Maze.Size();
    std::vector<bool> Shown(N * N, false);
    int Revealed = 0;
    for (const MazeStep& Step : Maze.GenerationSteps()) {
        RequireMaze(Step.Row >= 0 && Step.Row < N && Step.Col >= 0 && Step.Col < N &&
                        Maze.Cell(Step.Row, Step.Col) != '#',
                    "generation animation must not reveal walls or rejected layouts");
        const int Index = Step.Row * N + Step.Col;
        if (Shown[Index])
            continue;
        const bool Connected =
            (Step.Row > 0 && Shown[Index - N]) || (Step.Row + 1 < N && Shown[Index + N]) ||
            (Step.Col > 0 && Shown[Index - 1]) || (Step.Col + 1 < N && Shown[Index + 1]);
        RequireMaze((Revealed == 0 && Index == 0) || Connected,
                    "generation animation must grow a connected network from the spawn");
        Shown[Index] = true;
        ++Revealed;
    }
    for (int Row = 0; Row < N; ++Row)
        for (int Col = 0; Col < N; ++Col)
            RequireMaze(Shown[Row * N + Col] == (Maze.Cell(Row, Col) != '#'),
                        "generation preview must cover every passage in the selected layout");
    const auto& Last = Maze.GenerationSteps().back();
    RequireMaze(Last.Row == N - 1 && Last.Col == N - 1,
                "generation animation must still end at the exit");
}

// Regenerate a maze with the same dimensions and seed and compare both its final cells and
// recorded carving sequence. Reproducibility must cover the visible preview as well as the
// playable layout.
void CheckReproduction(const MazeData& Maze, unsigned CurrentSeed) {
    MazeData CurrentRepeated(Maze.Size(), CurrentSeed);
    for (int Row = 0; Row < Maze.Size(); ++Row)
        for (int Col = 0; Col < Maze.Size(); ++Col)
            RequireMaze(Maze.Cell(Row, Col) == CurrentRepeated.Cell(Row, Col),
                        "a run seed must reproduce the selected maze layout");
    const auto& Steps = Maze.GenerationSteps();
    const auto& CurrentReplay = CurrentRepeated.GenerationSteps();
    RequireMaze(Steps.size() == CurrentReplay.size(), "a seed must reproduce generation-step count");
    for (std::size_t Index = 0; Index < Steps.size(); ++Index)
        RequireMaze(Steps[Index].Row == CurrentReplay[Index].Row && Steps[Index].Col == CurrentReplay[Index].Col,
                    "a run seed must reproduce the carving animation");
}
} // namespace

// Check every supported dimension, including even-size exit connectors, and verify deterministic
// layouts and previews. Also guard against difficulty regressing into long routes with too few
// decisions.
void RunMazeGenerationTests() {
    for (int CurrentSize = MinSize; CurrentSize <= MaxSize; ++CurrentSize) {
        for (unsigned CurrentSeed : {0u, 1u, 1234u, 9917u}) {
            MazeData Maze(CurrentSize, CurrentSeed);
            CheckLayout(Maze);
            CheckAnimation(Maze);
            CheckReproduction(Maze, CurrentSeed);
        }
    }
    const int Sizes[] = {15, 25, 40};
    const int MinimumChoices[] = {4, 8, 14};
    for (int CurrentLevel = 0; CurrentLevel < 3; ++CurrentLevel) {
        int Choices = 0;
        for (unsigned CurrentSeed = 0; CurrentSeed < 128; ++CurrentSeed)
            Choices += CheckLayout(MazeData(Sizes[CurrentLevel], CurrentSeed));
        RequireMaze(Choices >= 128 * MinimumChoices[CurrentLevel],
                    "preset mazes must offer meaningful wrong turns along the exit route");
    }
}
