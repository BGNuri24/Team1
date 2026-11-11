#include "../logic/Game.h"
#include "../logic/GravityObstacles.h"
#include <cmath>
#include <stdexcept>

using namespace MazeGeometry;

// Throw a descriptive failure at the first unsafe obstacle layout or missed swept collision. The
// shared gameplay suite catches it and reports the failing barrier invariant.
static void RequireObstacle(bool Condition, const char* Message) {
    if (!Condition) throw std::runtime_error(Message);
}

// Generate deterministic hazards across many mazes and verify dimensions, safe spawn and exit
// areas, spacing, and usable flip zones. Then test body sweeps against floor and ceiling
// barriers, including fast and vertical crossings.
void RunObstacleTests() {
    for (int CurrentSize : {10, 11, 15, 25, 40, 50}) {
        for (unsigned CurrentSeed = 0; CurrentSeed < 20; ++CurrentSeed) {
            Game CurrentGame(CurrentSize, 1000, GameMode::ZeroGravity, CurrentSeed);
            Game CurrentRepeated(CurrentSize, 1000, GameMode::ZeroGravity, CurrentSeed);
            const auto& Obstacles = CurrentGame.Obstacles();
            RequireObstacle(Obstacles.size() >= 2 && Obstacles.size() == CurrentRepeated.Obstacles().size(),
                "gravity layouts must contain reproducible floor and ceiling barriers");
            for (std::size_t I = 0; I < Obstacles.size(); ++I) {
                const auto& Obstacle = Obstacles[I];
                const auto& CurrentReplay = CurrentRepeated.Obstacles()[I];
                RequireObstacle(Obstacle.Row == CurrentReplay.Row && Obstacle.Col == CurrentReplay.Col &&
                    Obstacle.AcrossX == CurrentReplay.AcrossX && Obstacle.Ceiling == CurrentReplay.Ceiling,
                    "the same run seed must reproduce every barrier");
                RequireObstacle(CurrentGame.Maze().Cell(Obstacle.Row, Obstacle.Col) == '.' &&
                    Obstacle.Row + Obstacle.Col >= 4 &&
                    (CurrentSize - 1 - Obstacle.Row) + (CurrentSize - 1 - Obstacle.Col) >= 3,
                    "barriers must preserve safe spawn and exit approaches");
                const int CurrentR = Obstacle.Row, CurrentC = Obstacle.Col;
                RequireObstacle(Obstacle.AcrossX
                    ? CurrentGame.Maze().Cell(CurrentR - 1, CurrentC) != '#' && CurrentGame.Maze().Cell(CurrentR + 1, CurrentC) != '#' &&
                        CurrentGame.Maze().Cell(CurrentR, CurrentC - 1) == '#' && CurrentGame.Maze().Cell(CurrentR, CurrentC + 1) == '#'
                    : CurrentGame.Maze().Cell(CurrentR, CurrentC - 1) != '#' && CurrentGame.Maze().Cell(CurrentR, CurrentC + 1) != '#' &&
                        CurrentGame.Maze().Cell(CurrentR - 1, CurrentC) == '#' && CurrentGame.Maze().Cell(CurrentR + 1, CurrentC) == '#',
                    "barriers must span straight corridors without obstructing a corner");
                for (std::size_t J = 0; J < I; ++J)
                    RequireObstacle(std::abs(CurrentR - Obstacles[J].Row) + std::abs(CurrentC - Obstacles[J].Col) >= 4,
                        "barriers must leave room to flip between blocked surfaces");
            }
        }
    }
    Game CurrentNormal(25, 1000, GameMode::Normal, 1234);
    RequireObstacle(CurrentNormal.Obstacles().empty(), "normal mode must contain no gravity hazards");

    for (bool AcrossX : {false, true}) {
        for (bool Ceiling : {false, true}) {
            const std::vector<GravityObstacle> CurrentBarrier = {{5, 5, AcrossX, Ceiling}};
            Vector3 Before = {CellCenter(5), Ceiling ? CeilingHeight - StandingEyeHeight : StandingEyeHeight,
                              CellCenter(5)};
            Vector3 After = Before;
            (AcrossX ? Before.z : Before.x) -= 0.8f;
            (AcrossX ? After.z : After.x) += 0.8f;
            RequireObstacle(HitsGravityObstacle(CurrentBarrier, Before, Ceiling, After, Ceiling),
                "a fast movement segment must not skip over the blocked surface");
            Before.y = After.y = Ceiling ? StandingEyeHeight : CeilingHeight - StandingEyeHeight;
            RequireObstacle(!HitsGravityObstacle(CurrentBarrier, Before, !Ceiling, After, !Ceiling),
                "the opposite surface must remain clear across the entire barrier");
            Vector3 CurrentFloor = {CellCenter(5), StandingEyeHeight, CellCenter(5)};
            Vector3 Roof = {CurrentFloor.x, CeilingHeight - StandingEyeHeight, CurrentFloor.z};
            RequireObstacle(HitsGravityObstacle(CurrentBarrier, CurrentFloor, false, Roof, true) &&
                HitsGravityObstacle(CurrentBarrier, Roof, true, CurrentFloor, false),
                "a gravity transfer through a barrier must be lethal in either direction");
        }
    }
}
