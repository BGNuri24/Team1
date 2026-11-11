#pragma once
#include "../data/GravityObstacle.h"
#include "../data/MazeData.h"
#include "raylib.h"
#include <cstdint>
#include <vector>

// Keep collision and rendering tied to the same physical dimensions in world units.
inline constexpr float GravityBarrierHeight = 0.44f;
inline constexpr float GravityBarrierDepth = 0.28f;

// Find the exit route with breadth-first search and select seeded, spaced barriers in straight
// passages away from spawn, exit, and turns. Alternate their blocked surfaces so the opposite
// surface remains available as a bypass.
std::vector<GravityObstacle> GenerateGravityObstacles(const MazeData& Maze, std::uint32_t CurrentSeed);

// Build the exact lethal world-space box from an obstacle's cell, orientation, and blocked
// surface. The box spans the corridor width; collision and rendering share it to keep visible
// hazards aligned.
BoundingBox GravityObstacleBounds(const GravityObstacle& Obstacle);

// Convert eye positions into body centers using each endpoint's floor or ceiling orientation.
// Expand barrier boxes by player half-extents and test the movement segment, detecting jumping,
// fast walking, and gravity transfers.
bool HitsGravityObstacle(const std::vector<GravityObstacle>& Obstacles, Vector3 FromEye,
                         bool FromCeiling, Vector3 ToEye, bool ToCeiling);
