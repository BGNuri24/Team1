#pragma once
#include <cmath>

// World-space dimensions shared by movement, surface attachment and rendering.
namespace MazeGeometry {
inline constexpr float CellSize = 1.8f;
inline constexpr float HalfCell = CellSize * 0.5f;
inline constexpr float CeilingHeight = 1.35f;
inline constexpr float StandingEyeHeight = 0.54f;
inline constexpr float PlayerRadius = 0.10f;
inline constexpr float SurfaceOffset = 0.22f;
inline constexpr float MovementSpeed = 4.0f;

inline constexpr float cellCenter(int index) { return index * CellSize; }
inline int cellIndex(float position) { return (int)std::floor(position / CellSize + 0.5f); }
}
