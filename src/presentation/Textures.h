#pragma once
#include "raylib.h"

// All textures used by the 3D view.
struct Textures {
    Texture2D wall;
    Texture2D floor;
};

// builds the textures (call after InitWindow)
Textures createTextures();
// frees them (call before CloseWindow)
void destroyTextures(Textures& t);