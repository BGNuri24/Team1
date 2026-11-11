#include "../logic/Game.h"
#include "../logic/Difficulty.h"
#include "../logic/MazeThemeSelection.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <vector>

using namespace MazeGeometry;
// Run the standalone JumpMovement cases and their integration through Game from the shared
// gameplay executable. Fail through descriptive exceptions while leaving user input and
// persistent saves untouched.
void RunJumpTests();
// Generate deterministic hazards across many mazes and verify dimensions, safe spawn and exit
// areas, spacing, and usable flip zones. Then test body sweeps against floor and ceiling
// barriers, including fast and vertical crossings.
void RunObstacleTests();

// Throw the supplied explanatory message when a gameplay invariant fails. The test executable's
// main catches the exception and returns failure rather than continuing with misleading later
// checks.
static void Require(bool Condition, const char* Message) {
    if (!Condition)
        throw std::runtime_error(Message);
}

// Ignore invalid frame durations so malformed timing input cannot move the run timer backward or
// make its state non-finite.
static void TestInvalidFrameDuration() {
    Game CurrentGame(10, 30.0f, GameMode::Normal, 1234);
    CurrentGame.Update({1, 0, 0, 0, -1.0f, false});
    CurrentGame.Update({1, 0, 0, 0, std::numeric_limits<float>::quiet_NaN(), false});
    Require(CurrentGame.TimeTaken() == 0.0f && CurrentGame.TimeLeft() == 30.0f && !CurrentGame.Finished(),
            "invalid frame durations must not change gameplay state");
}

// Check timer expiry and frozen results, then verify ceiling attachment and return-to-floor
// transitions in Zero Gravity.
static void TestTimerAndGravity() {
    Game Timed(10, 1.0f);
    Timed.Update({0, 0, 0, 0, 1.25f, false});
    Require(Timed.Lost() && Timed.GetFailureReason() == FailureReason::TimeExpired &&
                Timed.TimeLeft() == 0 && !Timed.Won(),
            "timer should end the run");
    Timed.Update({1, 1, 50, 50, 1.0f, true});
    Require(Timed.X() == 0 && Timed.Z() == 0 && Timed.TimeTaken() == 1.25f,
            "finished run must stay frozen");

    Game CurrentGravity(10, 30.0f, GameMode::ZeroGravity);
    Require(CurrentGravity.Mode() == GameMode::ZeroGravity, "gravity level must retain its mode");
    // Apply an upward look delta without advancing time so the following gravity toggle targets the
    // ceiling.
    CurrentGravity.Update({0, 0, 0, -400, 0, false});
    CurrentGravity.Update({0, 0, 0, 0, 0, true});
    Require(CurrentGravity.SurfaceAttached(), "G should switch directly to the ceiling");
    CurrentGravity.Update({0, 0, 0, 0, 0.25f, false});
    Require(CurrentGravity.CameraUp().y < -0.99f &&
                std::fabs(CurrentGravity.Y() - (CeilingHeight - StandingEyeHeight)) < 0.001f,
            "ceiling transition should finish with inverted up direction");
    CurrentGravity.Update({0, 0, 0, 0, 0, true});
    CurrentGravity.Update({0, 0, 0, 0, 0.25f, false});
    Require(!CurrentGravity.SurfaceAttached() && CurrentGravity.CameraUp().y > 0.99f &&
                CurrentGravity.Y() == StandingEyeHeight,
            "G should return to the floor");
}

// Confirm that Normal levels ignore gravity toggles and keep floor orientation when movement
// collides with a boundary.
static void TestNormalGravity() {
    constexpr float Pi = 3.14159265358979323846f;
    for (int CurrentSize : {10, 15, 25, 40, 50}) {
        Game CurrentNormal(CurrentSize, 1000.0f);
        Require(CurrentNormal.Mode() == GameMode::Normal, "new games should default to normal gravity");
        CurrentNormal.Update({0, 0, 0, -400, 0, false});
        CurrentNormal.Update({0, 0, 0, 0, 0.25f, true});
        Require(!CurrentNormal.SurfaceAttached() && CurrentNormal.CameraUp().y > 0.99f &&
                    CurrentNormal.Y() == StandingEyeHeight,
                "G must not attach to the ceiling in normal levels");
        CurrentNormal.Update({0, 0, 0, 400, 0, false});
        float CurrentTurn = std::remainder(-Pi / 2 - CurrentNormal.Yaw(), 2 * Pi);
        CurrentNormal.Update({0, 0, CurrentTurn / 0.0025f, 0, 0, false});
        for (int CurrentTick = 0; CurrentTick < 60; ++CurrentTick)
            CurrentNormal.Update({1, 0, 0, 0, 1.0f / 60, true});
        Require(!CurrentNormal.SurfaceAttached() && CurrentNormal.CameraUp().y > 0.99f &&
                    CurrentNormal.Y() == StandingEyeHeight,
                "walking into walls must not change gravity in normal levels");
        Require(CurrentNormal.Z() >= -HalfCell + PlayerRadius - 0.001f && CurrentNormal.Row() == 0,
                "normal movement must still collide with the maze boundary");
    }
}

// Side-wall contact must remain ordinary collision on both surfaces, even when G is pressed while
// facing that wall. A gravity flip preserves corridor heading and horizontal position.
static void TestZeroGravityWallCollision() {
    constexpr float Pi = 3.14159265358979323846f;
    for (bool Ceiling : {false, true}) {
        for (float Target : {-Pi / 2, Pi}) {
            Game CurrentGame(10, 30.0f, GameMode::ZeroGravity, 1234);
            if (Ceiling) {
                CurrentGame.Update({0, 0, 0, 0, 0, true});
                CurrentGame.Update({0, 0, 0, 0, .25f, false});
            }
            float CurrentTurn = std::remainder(Target - CurrentGame.Yaw(), 2 * Pi);
            CurrentGame.Update({0, 0, CurrentTurn / (.0025f * (Ceiling ? -1 : 1)), 0, 0, false});
            for (int CurrentTick = 0; CurrentTick < 60; ++CurrentTick)
                CurrentGame.Update({1, 0, 0, 0, .02f, false});
            Require(CurrentGame.SurfaceAttached() == Ceiling && std::fabs(CurrentGame.CameraUp().y) > .99f,
                    "left and right wall collisions must never change gravity");
            const Vector3 Before = CurrentGame.Position();
            const float Heading = CurrentGame.Yaw();
            CurrentGame.Update({0, 0, 0, 0, 0, true});
            CurrentGame.Update({0, 0, 0, 0, .25f, false});
            Require(CurrentGame.SurfaceAttached() != Ceiling && CurrentGame.X() == Before.x &&
                        CurrentGame.Z() == Before.z,
                    "G must flip vertically even when aiming directly at a side wall");
            Require(std::fabs(std::remainder(CurrentGame.Yaw() - Heading, 2 * Pi)) < .001f,
                    "flipping must preserve forward corridor heading");
        }
    }
}

// Move through a deterministic maze using ordinary walking and verify the widened passage remains
// usable. This guards the shared geometry and player-clearance rules against narrowing or
// collision regressions.
static void TestWideCorridorClearance() {
    Game CurrentGame(15, 1000, GameMode::Normal, 1234);
    constexpr float Pi = 3.14159265358979323846f;
    float CurrentTurn = std::remainder(-Pi / 2 - CurrentGame.Yaw(), 2 * Pi);
    CurrentGame.Update({0, 0, CurrentTurn / .0025f, 0, 0, false});
    CurrentGame.Update({1, 0, 0, 0, .7f / MovementSpeed, false});
    Require(CurrentGame.Z() < -.69f && CurrentGame.Row() == 0 && !CurrentGame.SurfaceAttached(),
            "the widened corridor must allow walking beyond the old half-unit wall");
    CurrentGame.Update({0, 0, Pi / .0025f, 0, 0, false});
    CurrentGame.Update({1, 0, 0, 0, 1.4f / MovementSpeed, false});
    Require(CurrentGame.Z() > .69f && CurrentGame.Row() == 0,
            "the corridor must provide usable space across its full new width");
    Require(CurrentGame.Y() == StandingEyeHeight, "widening must preserve normal standing height");
}

// Use breadth-first search to return a reachable passage path from the player's current cell to
// the target cell index.
static std::vector<int> PathTo(const Game& CurrentGame, int Target) {
    const int CurrentSize = CurrentGame.Maze().Size();
    const int CurrentStart = CurrentGame.Row() * CurrentSize + CurrentGame.Col();
    std::vector<int> Previous(CurrentSize * CurrentSize, -1);
    std::queue<int> Pending;
    Pending.push(CurrentStart);
    Previous[CurrentStart] = CurrentStart;
    const int Dr[] = {-1, 1, 0, 0}, Dc[] = {0, 0, -1, 1};
    while (!Pending.empty()) {
        int CurrentCell = Pending.front();
        Pending.pop();
        for (int Direction = 0; Direction < 4; ++Direction) {
            int Row = CurrentCell / CurrentSize + Dr[Direction], Col = CurrentCell % CurrentSize + Dc[Direction];
            if (CurrentGame.Maze().Cell(Row, Col) == '#')
                continue;
            int Next = Row * CurrentSize + Col;
            if (Previous[Next] < 0) {
                Previous[Next] = CurrentCell;
                Pending.push(Next);
            }
        }
    }
    Require(Previous[Target] >= 0, "destination must be reachable");
    std::vector<int> Path;
    for (int CurrentCell = Target; CurrentCell != CurrentStart; CurrentCell = Previous[CurrentCell])
        Path.push_back(CurrentCell);
    std::reverse(Path.begin(), Path.end());
    return Path;
}

// Follow passage waypoints through public game input, optionally taking inside corners, and
// assert that ordinary movement never attaches to a side wall.
static void FollowPath(Game& CurrentGame, const std::vector<int>& Path, int CollisionTarget = -1,
                       bool InsideCorners = false) {
    const int CurrentSize = CurrentGame.Maze().Size();
    const int StartCell = CurrentGame.Row() * CurrentSize + CurrentGame.Col();
    constexpr float Pi = 3.14159265358979323846f;
    for (int Index = 0; Index < (int)Path.size(); ++Index) {
        const int CurrentCell = Path[Index];
        if (CurrentGame.Finished())
            break;
        for (const GravityObstacle& Obstacle : CurrentGame.Obstacles()) {
            if (Obstacle.Row * CurrentSize + Obstacle.Col != CurrentCell)
                continue;
            const bool Ceiling = CurrentCell == CollisionTarget ? Obstacle.Ceiling : !Obstacle.Ceiling;
            if (CurrentGame.SurfaceAttached() != Ceiling) {
                CurrentGame.Update({0, 0, 0, 0, 0, true});
                CurrentGame.Update({0, 0, 0, 0, .25f, false});
            }
        }
        float TargetX = CellCenter(CurrentCell % CurrentSize), TargetZ = CellCenter(CurrentCell / CurrentSize);
        if (InsideCorners && Index + 1 < (int)Path.size()) {
            const int Previous = Index > 0 ? Path[Index - 1] : StartCell;
            const int Next = Path[Index + 1];
            const int InX = CurrentCell % CurrentSize - Previous % CurrentSize, InZ = CurrentCell / CurrentSize - Previous / CurrentSize;
            const int OutX = Next % CurrentSize - CurrentCell % CurrentSize, OutZ = Next / CurrentSize - CurrentCell / CurrentSize;
            if (InX * OutX + InZ * OutZ == 0) {
                // Stay inside the corner cell with full player clearance. Every segment still
                // uses real movement and collision, rather than teleporting or skipping time.
                const float Inset = HalfCell - PlayerRadius - .02f;
                TargetX += (OutX - InX) * Inset;
                TargetZ += (OutZ - InZ) * Inset;
            }
        }
        float Dx = TargetX - CurrentGame.X(), Dz = TargetZ - CurrentGame.Z();
        float CurrentTurn = std::remainder(std::atan2(Dz, Dx) - CurrentGame.Yaw(), 2 * Pi);
        CurrentGame.Update({0, 0, CurrentTurn / (0.0025f * CurrentGame.CameraUp().y), 0, 0, false});
        float Distance = std::sqrt(Dx * Dx + Dz * Dz);
        const int Ticks = CurrentCell == CollisionTarget ? 1 : 10;
        for (int CurrentTick = 0; CurrentTick < Ticks && !CurrentGame.Finished(); ++CurrentTick)
            CurrentGame.Update({1, 0, 0, 0, Distance / (Ticks * MovementSpeed), false});
        Require(std::fabs(CurrentGame.CameraUp().y) > .99f, "path movement must stay on floor or ceiling");
        if (InsideCorners && !CurrentGame.Finished())
            Require(std::hypot(CurrentGame.X() - TargetX, CurrentGame.Z() - TargetZ) < .005f,
                    "an inside-corner route must reach its waypoint through normal collision");
    }
}

// Reach a corridor with open space beyond a wall and confirm that a slow movement frame cannot
// cross the solid cell.
static void TestNoWallTunneling() {
    Game CurrentGame(25, 10000, GameMode::Normal, 1234);
    const int CurrentSize = CurrentGame.Maze().Size();
    const int Dr[] = {-1, 1, 0, 0}, Dc[] = {0, 0, -1, 1};
    int Origin = -1, Direction = -1;
    for (int Row = 1; Row < CurrentSize - 1 && Origin < 0; ++Row)
        for (int Col = 1; Col < CurrentSize - 1 && Origin < 0; ++Col) {
            if (CurrentGame.Maze().Cell(Row, Col) == '#')
                continue;
            for (int CurrentD = 0; CurrentD < 4; ++CurrentD) {
                if (CurrentGame.Maze().Cell(Row + Dr[CurrentD], Col + Dc[CurrentD]) == '#' &&
                    CurrentGame.Maze().Cell(Row + 2 * Dr[CurrentD], Col + 2 * Dc[CurrentD]) == '.') {
                    Origin = Row * CurrentSize + Col;
                    Direction = CurrentD;
                    break;
                }
            }
        }
    Require(Origin >= 0, "test maze needs a wall with an open corridor beyond it");
    FollowPath(CurrentGame, PathTo(CurrentGame, Origin));
    Require(CurrentGame.Row() == Origin / CurrentSize && CurrentGame.Col() == Origin % CurrentSize,
            "wall collision fixture should reach its starting corridor");
    constexpr float Pi = 3.14159265358979323846f;
    float CurrentTurn =
        std::remainder(std::atan2((float)Dr[Direction], (float)Dc[Direction]) - CurrentGame.Yaw(), 2 * Pi);
    CurrentGame.Update({0, 0, CurrentTurn / .0025f, 0, 0, false});
    CurrentGame.Update({1, 0, 0, 0, 2 * CellSize / MovementSpeed, false});
    Require(CurrentGame.Row() == Origin / CurrentSize && CurrentGame.Col() == Origin % CurrentSize && !CurrentGame.SurfaceAttached(),
            "a slow frame must not jump through an entire wall into another corridor");
}

// Construct a reproducible run for the requested size and mode, compute an exit route, and follow
// it through public input. Verify victory and confirm later updates cannot change the finished
// timers.
static void TestReachableExit(int CurrentSize, GameMode Mode = GameMode::Normal, unsigned CurrentSeed = 1234) {
    Game CurrentGame(CurrentSize, 10000.0f, Mode, CurrentSeed);
    FollowPath(CurrentGame, PathTo(CurrentGame, CurrentSize * CurrentSize - 1));
    Require(CurrentGame.Won() && !CurrentGame.Lost() && CurrentGame.TimeLeft() > 0,
            "following a valid path must win before the timer expires");
    const float Elapsed = CurrentGame.TimeTaken(), Remaining = CurrentGame.TimeLeft();
    CurrentGame.Update({1, 1, 100, 100, 1, true});
    Require(CurrentGame.TimeTaken() == Elapsed && CurrentGame.TimeLeft() == Remaining,
            "winning must freeze both elapsed and remaining time");
}

// Exercise real movement and gravity transfers with the unchanged preset timers. Stronger
// branching must still leave enough time for a player who follows the correct route. Take corners
// inside their clearance, as a player can, instead of adding a detour through every cell center.
static void TestPresetCompletion() {
    for (unsigned CurrentSeed = 0; CurrentSeed < 32; ++CurrentSeed) {
        for (int CurrentLevel = 0; CurrentLevel <= DifficultyCount; ++CurrentLevel) {
            const bool CurrentZeroGravity = CurrentLevel == DifficultyCount;
            const Difficulty& CurrentDifficulty =
                CurrentZeroGravity ? GetZeroGravityLevel() : GetDifficulty(CurrentLevel);
            Game CurrentGame(CurrentDifficulty.CurrentSize, CurrentDifficulty.Seconds,
                      CurrentZeroGravity ? GameMode::ZeroGravity : GameMode::Normal, CurrentSeed);
            FollowPath(CurrentGame, PathTo(CurrentGame, CurrentDifficulty.CurrentSize * CurrentDifficulty.CurrentSize - 1), -1, true);
            Require(CurrentGame.Won() && CurrentGame.TimeLeft() > 0,
                    "preset mazes must remain completable within their original timers");
        }
    }
}

// Deliberately cross each barrier type on its blocked surface and verify immediate failure, a
// frozen result, and the correct failure reason.
static void TestObstacleFailure() {
    for (bool Ceiling : {false, true}) {
        Game CurrentGame(25, 10000, GameMode::ZeroGravity, 1234);
        auto Target = std::find_if(
            CurrentGame.Obstacles().begin(), CurrentGame.Obstacles().end(),
            // Find a generated barrier on the surface this failure case intends to cross. The
            // test follows that target through public movement input instead of assigning a loss
            // flag directly.
            [&](const GravityObstacle& Obstacle) { return Obstacle.Ceiling == Ceiling; });
        Require(Target != CurrentGame.Obstacles().end(), "fixture needs both barrier types");
        const int CurrentCell = Target->Row * CurrentGame.Maze().Size() + Target->Col;
        FollowPath(CurrentGame, PathTo(CurrentGame, CurrentCell), CurrentCell);
        Require(CurrentGame.Lost() && !CurrentGame.Won() && CurrentGame.GetFailureReason() == FailureReason::ObstacleHit &&
                    CurrentGame.TimeLeft() > 0,
                "touching either barrier type must fail the run");
        const Vector3 Stopped = CurrentGame.Position();
        const float CurrentTime = CurrentGame.TimeTaken();
        CurrentGame.Update({1, 1, 30, 20, 1, true, true});
        Require(CurrentGame.TimeTaken() == CurrentTime && CurrentGame.X() == Stopped.x && CurrentGame.Y() == Stopped.y &&
                    CurrentGame.Z() == Stopped.z,
                "a failed run must freeze all movement, gravity and timing");
    }
}

// Passing below a ceiling barrier is safe, but jumping or flipping into that same barrier must
// fail immediately rather than allowing the camera transfer to bypass collision.
static void TestVerticalObstacleCollision() {
    for (bool CurrentJump : {false, true}) {
        Game CurrentGame(25, 10000, GameMode::ZeroGravity, 1234);
        auto Target =
            std::find_if(CurrentGame.Obstacles().begin(), CurrentGame.Obstacles().end(),
                         // Select a ceiling barrier for vertical-collision regression checks. The
                         // case verifies that walking below it is safe but jumping or
                         // transferring into it is lethal.
                         [](const GravityObstacle& Obstacle) { return Obstacle.Ceiling; });
        Require(Target != CurrentGame.Obstacles().end(), "fixture needs a ceiling barrier");
        FollowPath(CurrentGame, PathTo(CurrentGame, Target->Row * CurrentGame.Maze().Size() + Target->Col));
        Require(!CurrentGame.Finished() && !CurrentGame.SurfaceAttached(),
                "the floor below a ceiling barrier is safe");
        CurrentGame.Update({0, 0, 0, 0, CurrentJump ? .15f : 0.0f, !CurrentJump, CurrentJump});
        Require(CurrentGame.Lost() && CurrentGame.GetFailureReason() == FailureReason::ObstacleHit,
                "jumping or flipping into a ceiling barrier must fail");
    }
}

// Run jump, gravity, clearance and route-completion checks across supported sizes with
// deterministic maze randomness. Cover every non-repeating random-draw mapping and ensure visual
// randomness leaves maze carving unchanged.
static void TestRunThemes() {
    for (std::uint32_t CurrentSeed = 0; CurrentSeed < 20; ++CurrentSeed) {
        Game CurrentNormal(10, 100, GameMode::Normal, CurrentSeed);
        Game CurrentZero(10, 100, GameMode::ZeroGravity, CurrentSeed);
        Require(CurrentNormal.Theme() == ActiveMazeTheme && CurrentZero.Theme() == ActiveMazeTheme,
                "new runs must use the curated palette regardless of maze seed or game mode");
    }
    for (int Previous = -1; Previous < MazeThemeCount; ++Previous) {
        std::array<bool, MazeThemeCount> Found{};
        const int Choices = Previous < 0 ? MazeThemeCount : MazeThemeCount - 1;
        for (int CurrentDraw = 0; CurrentDraw < Choices; ++CurrentDraw) {
            const int CurrentSelected = (int)ThemeFromDraw(CurrentDraw, Previous);
            Require(CurrentSelected >= 0 && CurrentSelected < MazeThemeCount && CurrentSelected != Previous &&
                        !Found[CurrentSelected],
                    "random draws must map uniquely to valid themes excluding the previous one");
            Found[CurrentSelected] = true;
        }
    }
    MazeData Baseline(15, 9917);
    Game Themed(15, 100, GameMode::Normal, 9917);
    Game Reproduced(15, 100, GameMode::Normal, Themed.Seed());
    Require(Themed.Theme() == Reproduced.Theme(), "the same seed must reproduce its default theme");
    for (int Row = 0; Row < 15; ++Row)
        for (int Col = 0; Col < 15; ++Col)
            Require(Themed.Maze().Cell(Row, Col) == Baseline.Cell(Row, Col) &&
                        Reproduced.Maze().Cell(Row, Col) == Baseline.Cell(Row, Col),
                    "choosing a visual theme must not change the maze-carving random sequence");
    const MazeTheme CurrentChosen = Themed.Theme();
    for (int Frame = 0; Frame < 20; ++Frame)
        Themed.Update({0, 0, 0, 0, 1.0f / 60, false});
    Require(Themed.Theme() == CurrentChosen, "a run's palette must stay fixed during gameplay updates");
    MazeThemeSelector Selector(1234), CurrentRepeated(1234);
    MazeTheme Previous = Selector.Next();
    Require(CurrentRepeated.Next() == Previous, "seeded theme sequences must be reproducible");
    for (int CurrentRun = 0; CurrentRun < 12; ++CurrentRun) {
        Game Next(10, 100, CurrentRun % 2 ? GameMode::Normal : GameMode::ZeroGravity, CurrentRun,
                  Selector.Next());
        Require(Next.Theme() == CurrentRepeated.Next(),
                "an independent seeded selector must reproduce every choice");
        Require(Next.Theme() != Previous && (int)Next.Theme() >= 0 &&
                    (int)Next.Theme() < MazeThemeCount,
                "new runs must choose an allowed theme without immediately repeating one");
        Previous = Next.Theme();
    }

    RunThemeSelector Previews(4321), DirectRestarts(4321);
    MazeThemeSelector Expected(4321);
    std::optional<MazeTheme> LastRun;
    for (int CurrentRun = 0; CurrentRun < 1000; ++CurrentRun) {
        const MazeTheme Preview = Previews.Preview();
        for (int Navigation = 0; Navigation < 20; ++Navigation)
            Require(Previews.Preview() == Preview,
                    "menu navigation must keep the upcoming theme stable");
        const MazeTheme Started = Previews.StartRun();
        Require(Started == Preview && Started == Expected.Next(),
                "a run must use the previewed theme without another random draw");
        Require(DirectRestarts.StartRun() == Started,
                "restarting without previewing must follow the same seeded theme sequence");
        Require(!LastRun || Started != *LastRun,
                "previewed runs and direct restarts must not immediately repeat a theme");
        LastRun = Started;
    }
}

// Run movement, jump, obstacle, gravity, theme, timer, and reachable-exit regression checks.
// Catch the first descriptive failure and return a nonzero process status for the test runner.
int main() {
    try {
        TestRunThemes();
        RunJumpTests();
        RunObstacleTests();
        TestObstacleFailure();
        TestVerticalObstacleCollision();
        TestTimerAndGravity();
        TestInvalidFrameDuration();
        TestNormalGravity();
        TestZeroGravityWallCollision();
        TestWideCorridorClearance();
        TestNoWallTunneling();
        TestPresetCompletion();
        for (int CurrentSize : {10, 11, 15, 25, 40, 50}) {
            TestReachableExit(CurrentSize);
            for (unsigned CurrentSeed : {0u, 1u, 1234u, 9917u})
                TestReachableExit(CurrentSize, GameMode::ZeroGravity, CurrentSeed);
        }
    } catch (const std::exception& Error) {
        std::cerr << Error.what() << '\n';
        return 1;
    }
    std::cout << "Gameplay tests passed.\n";
}
