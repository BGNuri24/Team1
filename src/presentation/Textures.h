#pragma once
#include "raylib.h"

// All textures used by the 3D view.
struct Textures {
    Texture2D wall;
    Texture2D floor;
    Texture2D ceiling;
    // Four 512x128 signs stacked vertically, with transparent padding per slot.
    Texture2D decals;
};

// builds the textures (call after InitWindow)
Textures createTextures();
// frees them (call before CloseWindow)
void destroyTextures(Textures& t);
