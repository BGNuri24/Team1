#include "GravityObstacles.h"
#include "../data/MazeGeometry.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <random>

using namespace MazeGeometry;

// Find the exit route with breadth-first search and select seeded, spaced barriers in straight
// passages away from spawn, exit, and turns. Alternate their blocked surfaces so the opposite
// surface remains available as a bypass.
std::vector<GravityObstacle> GenerateGravityObstacles(const MazeData& Maze, std::uint32_t CurrentSeed) {
    const int CurrentSize = Maze.Size();
    std::vector<int> Previous(CurrentSize * CurrentSize, -1);
    std::queue<int> Pending;
    Pending.push(0);
    Previous[0] = 0;
    const int Dr[] = {-1, 1, 0, 0}, Dc[] = {0, 0, -1, 1};
    while (!Pending.empty()) {
        const int CurrentCell = Pending.front();
        Pending.pop();
        for (int CurrentSide = 0; CurrentSide < 4; ++CurrentSide) {
            const int Row = CurrentCell / CurrentSize + Dr[CurrentSide], Col = CurrentCell % CurrentSize + Dc[CurrentSide];
            if (Maze.Cell(Row, Col) == '#') continue;
            const int Next = Row * CurrentSize + Col;
            if (Previous[Next] < 0) {
                Previous[Next] = CurrentCell;
                Pending.push(Next);
            }
        }
    }
    std::vector<bool> CurrentRoute(CurrentSize * CurrentSize, false);
    for (int CurrentCell = CurrentSize * CurrentSize - 1; CurrentCell > 0 && Previous[CurrentCell] >= 0; CurrentCell = Previous[CurrentCell])
        CurrentRoute[CurrentCell] = true;

    std::vector<GravityObstacle> Candidates;
    for (int Row = 0; Row < CurrentSize; ++Row) {
        for (int Col = 0; Col < CurrentSize; ++Col) {
            if (Maze.Cell(Row, Col) == '#' || Row + Col < 4 ||
                (CurrentSize - 1 - Row) + (CurrentSize - 1 - Col) < 3) continue;
            const bool North = Maze.Cell(Row - 1, Col) != '#';
            const bool South = Maze.Cell(Row + 1, Col) != '#';
            const bool West = Maze.Cell(Row, Col - 1) != '#';
            const bool East = Maze.Cell(Row, Col + 1) != '#';
            if (North && South && !West && !East)
                Candidates.push_back({Row, Col, true, false});
            else if (West && East && !North && !South)
                Candidates.push_back({Row, Col, false, false});
        }
    }
    std::mt19937 Random(CurrentSeed ^ 0xa3817c25u);
    std::shuffle(Candidates.begin(), Candidates.end(), Random);
    // Prioritize candidate barriers that lie on the computed exit route while preserving order
    // within both groups. The predicate reads the route mask and leaves candidate geometry
    // unchanged.
    std::stable_partition(Candidates.begin(), Candidates.end(), [&](const GravityObstacle& Item) {
        return CurrentRoute[Item.Row * CurrentSize + Item.Col];
    });
    std::vector<GravityObstacle> Result;
    const int Target = std::clamp(CurrentSize * CurrentSize / 60, 3, 24);
    for (GravityObstacle Candidate : Candidates) {
        // Reject a candidate when an accepted barrier is fewer than four Manhattan cells away.
        // This preserves enough corridor space between hazards for a floor-to-ceiling transfer.
        const bool Crowded = std::any_of(Result.begin(), Result.end(), [&](const GravityObstacle& CurrentOther) {
            return std::abs(Candidate.Row - CurrentOther.Row) + std::abs(Candidate.Col - CurrentOther.Col) < 4;
        });
        if (Crowded) continue;
        Candidate.Ceiling = Result.size() % 2 != 0;
        Result.push_back(Candidate);
        if ((int)Result.size() >= Target) break;
    }
    return Result;
}

// Build the exact lethal world-space box from an obstacle's cell, orientation, and blocked
// surface. The box spans the corridor width; collision and rendering share it to keep visible
// hazards aligned.
BoundingBox GravityObstacleBounds(const GravityObstacle& Obstacle) {
    const float CurrentX = CellCenter(Obstacle.Col), CurrentZ = CellCenter(Obstacle.Row);
    const float HalfX = Obstacle.AcrossX ? HalfCell : GravityBarrierDepth * 0.5f;
    const float HalfZ = Obstacle.AcrossX ? GravityBarrierDepth * 0.5f : HalfCell;
    const float Bottom = Obstacle.Ceiling ? CeilingHeight - GravityBarrierHeight : 0.0f;
    return {{CurrentX - HalfX, Bottom, CurrentZ - HalfZ},
            {CurrentX + HalfX, Bottom + GravityBarrierHeight, CurrentZ + HalfZ}};
}

// Intersect a movement segment with an axis-aligned box using overlapping entry and exit
// intervals on all three axes. Parallel segments are rejected only outside the corresponding
// slab, catching fast crossings between clear endpoints.
static bool SegmentHitsBox(Vector3 From, Vector3 To, BoundingBox Box) {
    float CurrentEnter = 0.0f, CurrentLeave = 1.0f;
    const float Origin[] = {From.x, From.y, From.z};
    const float Delta[] = {To.x - From.x, To.y - From.y, To.z - From.z};
    const float Minimum[] = {Box.min.x, Box.min.y, Box.min.z};
    const float Maximum[] = {Box.max.x, Box.max.y, Box.max.z};
    for (int Axis = 0; Axis < 3; ++Axis) {
        if (std::fabs(Delta[Axis]) < 0.000001f) {
            if (Origin[Axis] < Minimum[Axis] || Origin[Axis] > Maximum[Axis]) return false;
        } else {
            float Near = (Minimum[Axis] - Origin[Axis]) / Delta[Axis];
            float Far = (Maximum[Axis] - Origin[Axis]) / Delta[Axis];
            if (Near > Far) std::swap(Near, Far);
            CurrentEnter = std::max(CurrentEnter, Near);
            CurrentLeave = std::min(CurrentLeave, Far);
            if (CurrentEnter > CurrentLeave) return false;
        }
    }
    return true;
}

// Convert eye positions into body centers using each endpoint's floor or ceiling orientation.
// Expand barrier boxes by player half-extents and test the movement segment, detecting jumping,
// fast walking, and gravity transfers.
bool HitsGravityObstacle(const std::vector<GravityObstacle>& Obstacles, Vector3 FromEye,
                         bool FromCeiling, Vector3 ToEye, bool ToCeiling) {
    const float CurrentOffset = (StandingEyeHeight - PlayerRadius) * 0.5f;
    const float HalfHeight = (StandingEyeHeight + PlayerRadius) * 0.5f;
    FromEye.y += FromCeiling ? CurrentOffset : -CurrentOffset;
    ToEye.y += ToCeiling ? CurrentOffset : -CurrentOffset;
    for (const GravityObstacle& Obstacle : Obstacles) {
        BoundingBox Box = GravityObstacleBounds(Obstacle);
        Box.min.x -= PlayerRadius;
        Box.max.x += PlayerRadius;
        Box.min.z -= PlayerRadius;
        Box.max.z += PlayerRadius;
        Box.min.y -= HalfHeight;
        Box.max.y += HalfHeight;
        if (SegmentHitsBox(FromEye, ToEye, Box)) return true;
    }
    return false;
}
