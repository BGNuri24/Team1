#include "Input.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

// Keep simulation and cursor capture inactive while the user is outside the game window.
bool gameplayWindowActive() { return IsWindowFocused() && !IsWindowMinimized(); }

// Give pause requests priority over the current frame's movement and countdown updates.
bool gameplayPauseRequested() { return !gameplayWindowActive() || IsKeyPressed(KEY_ESCAPE); }

// Collect movement, sensitivity-scaled mouse deltas, frame seconds and fresh gravity/jump requests
// without changing the game state.
InputState readInput(float sensitivity) {
    InputState in;
    in.forward = (float)(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        - (float)(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    in.right = (float)(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        - (float)(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    Vector2 md = GetMouseDelta();
    const float multiplier = std::isfinite(sensitivity)
        ? std::clamp(sensitivity, 0.25f, 3.0f) : 1.0f;
    in.mouseDX = md.x * multiplier;
    in.mouseDY = md.y * multiplier;
    in.dt = GetFrameTime();
    in.gravityToggle = IsKeyPressed(KEY_G);
    in.jumpPressed = IsKeyPressed(KEY_SPACE) || GetMouseWheelMove() != 0;
    in.mapPressed = IsKeyPressed(KEY_E);
    return in;
}

// Move selection through a nonempty option list with wraparound; opposing navigation presses cancel
// each other.
void updateMenuSelection(int& selected, int count) {
    if (count <= 0) return;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % count;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + count - 1) % count;
}

// Report a fresh Enter or Space press as confirmation for menu navigation.
bool menuConfirmed() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

// Consume queued characters accepted by the supplied filter up to the length limit, then handle one
// backspace deletion and report any change.
bool editText(std::string& text, int maxLength, bool (*allowed)(int)) {
    bool changed = false;
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if (allowed(ch) && (int)text.size() < maxLength) {
            text += (char)ch;
            changed = true;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
        text.pop_back();
        changed = true;
    }
    return changed;
}
