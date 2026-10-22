#include "Input.h"
#include "raylib.h"

InputState readInput() {
    InputState in;
    in.forward = (float)(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        - (float)(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    in.right = (float)(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        - (float)(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    Vector2 md = GetMouseDelta();
    in.mouseDX = md.x;
    in.mouseDY = md.y;
    in.dt = GetFrameTime();
    in.gravityToggle = IsKeyPressed(KEY_G);
    return in;
}