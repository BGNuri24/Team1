#pragma once
#include "../data/MazeTheme.h"
#include "raylib.h"
#include <array>

// Preserve the reference swatches and derive consistent material, trim and lighting roles from
// them.
struct MazePalette {
    const char* name;
    std::array<Color, 5> swatches;
    Color wall, pattern, accent, trim, floor, ceiling, lamp, fog, paper;
};

// Return the curated material palette for a MazeTheme identifier. Palette lookup is deterministic
// and does not reroll a run, upload textures, or alter the currently applied menu colors.
const MazePalette& mazePalette(MazeTheme theme);
