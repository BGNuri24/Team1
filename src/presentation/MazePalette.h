#pragma once
#include "../data/MazeTheme.h"
#include "raylib.h"
#include <array>

// Preserve the reference swatches and derive consistent material, trim and lighting roles from them.
struct MazePalette {
    const char* name;
    std::array<Color,5> swatches;
    Color wall, pattern, accent, trim, floor, ceiling, lamp, fog, paper;
};

// Return the immutable palette for a supported theme, with Slate as a defensive fallback.
const MazePalette& mazePalette(MazeTheme theme);
