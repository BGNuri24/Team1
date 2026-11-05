#pragma once
#include "../data/GravityObstacle.h"
#include "../data/MazeData.h"
#include "raylib.h"
#include <cstdint>
#include <vector>

// Keep collision and rendering tied to the same physical dimensions in world units.
inline constexpr float GravityBarrierHeight = 0.44f;
inline constexpr float GravityBarrierDepth = 0.28f;

// Prefer barriers on the exit route, leaving the spawn, exit, corners and nearby flip zones clear.
std::vector<GravityObstacle> generateGravityObstacles(const MazeData& maze, std::uint32_t seed);

// Return the exact lethal volume shared by the renderer and movement collision checks.
BoundingBox gravityObstacleBounds(const GravityObstacle& obstacle);

// Sweep the player's body between two poses so jumping, fast movement and gravity flips cannot
// skip through a barrier. Ceiling flags describe the body's orientation at each endpoint.
bool hitsGravityObstacle(const std::vector<GravityObstacle>& obstacles, Vector3 fromEye,
                         bool fromCeiling, Vector3 toEye, bool toCeiling);
