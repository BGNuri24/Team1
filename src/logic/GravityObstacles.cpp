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
std::vector<GravityObstacle> generateGravityObstacles(const MazeData& maze, std::uint32_t seed) {
    const int size = maze.size();
    std::vector<int> previous(size * size, -1);
    std::queue<int> pending;
    pending.push(0);
    previous[0] = 0;
    const int dr[] = {-1, 1, 0, 0}, dc[] = {0, 0, -1, 1};
    while (!pending.empty()) {
        const int cell = pending.front();
        pending.pop();
        for (int side = 0; side < 4; ++side) {
            const int row = cell / size + dr[side], col = cell % size + dc[side];
            if (maze.cell(row, col) == '#') continue;
            const int next = row * size + col;
            if (previous[next] < 0) {
                previous[next] = cell;
                pending.push(next);
            }
        }
    }
    std::vector<bool> route(size * size, false);
    for (int cell = size * size - 1; cell > 0 && previous[cell] >= 0; cell = previous[cell])
        route[cell] = true;

    std::vector<GravityObstacle> candidates;
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (maze.cell(row, col) == '#' || row + col < 4 ||
                (size - 1 - row) + (size - 1 - col) < 3) continue;
            const bool north = maze.cell(row - 1, col) != '#';
            const bool south = maze.cell(row + 1, col) != '#';
            const bool west = maze.cell(row, col - 1) != '#';
            const bool east = maze.cell(row, col + 1) != '#';
            if (north && south && !west && !east)
                candidates.push_back({row, col, true, false});
            else if (west && east && !north && !south)
                candidates.push_back({row, col, false, false});
        }
    }
    std::mt19937 random(seed ^ 0xa3817c25u);
    std::shuffle(candidates.begin(), candidates.end(), random);
    // Prioritize candidate barriers that lie on the computed exit route while preserving order
    // within both groups. The predicate reads the route mask and leaves candidate geometry
    // unchanged.
    std::stable_partition(candidates.begin(), candidates.end(), [&](const GravityObstacle& item) {
        return route[item.row * size + item.col];
    });
    std::vector<GravityObstacle> result;
    const int target = std::clamp(size * size / 60, 3, 24);
    for (GravityObstacle candidate : candidates) {
        // Reject a candidate when an accepted barrier is fewer than four Manhattan cells away.
        // This preserves enough corridor space between hazards for a floor-to-ceiling transfer.
        const bool crowded = std::any_of(result.begin(), result.end(), [&](const GravityObstacle& other) {
            return std::abs(candidate.row - other.row) + std::abs(candidate.col - other.col) < 4;
        });
        if (crowded) continue;
        candidate.ceiling = result.size() % 2 != 0;
        result.push_back(candidate);
        if ((int)result.size() >= target) break;
    }
    return result;
}

// Build the exact lethal world-space box from an obstacle's cell, orientation, and blocked
// surface. The box spans the corridor width; collision and rendering share it to keep visible
// hazards aligned.
BoundingBox gravityObstacleBounds(const GravityObstacle& obstacle) {
    const float x = cellCenter(obstacle.col), z = cellCenter(obstacle.row);
    const float halfX = obstacle.acrossX ? HalfCell : GravityBarrierDepth * 0.5f;
    const float halfZ = obstacle.acrossX ? GravityBarrierDepth * 0.5f : HalfCell;
    const float bottom = obstacle.ceiling ? CeilingHeight - GravityBarrierHeight : 0.0f;
    return {{x - halfX, bottom, z - halfZ},
            {x + halfX, bottom + GravityBarrierHeight, z + halfZ}};
}

// Intersect a movement segment with an axis-aligned box using overlapping entry and exit
// intervals on all three axes. Parallel segments are rejected only outside the corresponding
// slab, catching fast crossings between clear endpoints.
static bool segmentHitsBox(Vector3 from, Vector3 to, BoundingBox box) {
    float enter = 0.0f, leave = 1.0f;
    const float origin[] = {from.x, from.y, from.z};
    const float delta[] = {to.x - from.x, to.y - from.y, to.z - from.z};
    const float minimum[] = {box.min.x, box.min.y, box.min.z};
    const float maximum[] = {box.max.x, box.max.y, box.max.z};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(delta[axis]) < 0.000001f) {
            if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) return false;
        } else {
            float near = (minimum[axis] - origin[axis]) / delta[axis];
            float far = (maximum[axis] - origin[axis]) / delta[axis];
            if (near > far) std::swap(near, far);
            enter = std::max(enter, near);
            leave = std::min(leave, far);
            if (enter > leave) return false;
        }
    }
    return true;
}

// Convert eye positions into body centers using each endpoint's floor or ceiling orientation.
// Expand barrier boxes by player half-extents and test the movement segment, detecting jumping,
// fast walking, and gravity transfers.
bool hitsGravityObstacle(const std::vector<GravityObstacle>& obstacles, Vector3 fromEye,
                         bool fromCeiling, Vector3 toEye, bool toCeiling) {
    const float offset = (StandingEyeHeight - PlayerRadius) * 0.5f;
    const float halfHeight = (StandingEyeHeight + PlayerRadius) * 0.5f;
    fromEye.y += fromCeiling ? offset : -offset;
    toEye.y += toCeiling ? offset : -offset;
    for (const GravityObstacle& obstacle : obstacles) {
        BoundingBox box = gravityObstacleBounds(obstacle);
        box.min.x -= PlayerRadius;
        box.max.x += PlayerRadius;
        box.min.z -= PlayerRadius;
        box.max.z += PlayerRadius;
        box.min.y -= halfHeight;
        box.max.y += halfHeight;
        if (segmentHitsBox(fromEye, toEye, box)) return true;
    }
    return false;
}
