#pragma once
#include <cmath>

// Keep collision, camera placement, movement speed and rendered geometry in the same world-space scale.
namespace MazeGeometry {
inline constexpr float CellSize = 1.8f;
inline constexpr float HalfCell = CellSize * 0.5f;
inline constexpr float CeilingHeight = 1.35f;
inline constexpr float StandingEyeHeight = 0.54f;
inline constexpr float PlayerRadius = 0.10f;
inline constexpr float SurfaceOffset = 0.22f;
// Ordinary floor and ceiling walking; bunny-hop tuning is independent in JumpMovement.
inline constexpr float MovementSpeed = 3.24f;

// Convert a row or column index into its world-space center using CellSize. Cell zero is centered
// at coordinate zero; rendering, spawning, and collision share this conversion.
inline constexpr float cellCenter(int index) { return index * CellSize; }
// Convert a world-space X or Z coordinate into the index of the nearest cell center. The
// half-cell offset and floor operation keep negative positions near the outer boundary consistent
// with collision checks.
inline int cellIndex(float position) { return (int)std::floor(position / CellSize + 0.5f); }
}
