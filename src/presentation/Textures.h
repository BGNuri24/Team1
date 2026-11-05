#pragma once
#include "raylib.h"
#include "MazePalette.h"

// Group the GPU texture handles used for maze surfaces and decorative wall signs.
struct Textures {
    Texture2D wall{};
    Texture2D floor{};
    Texture2D ceiling{};
    // Store four 512-by-128 sign slots in one 512-by-512 atlas; transparent padding separates each sign
    // from its slot boundary.
    Texture2D decals{};
};

// Generate and upload the maze materials after InitWindow has created a valid graphics context.
Textures createTextures(const MazePalette& palette = mazePalette(MazeTheme::Slate));
// Unload the texture handles before CloseWindow destroys the graphics context.
void destroyTextures(Textures& t);
