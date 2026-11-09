#include "Input.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

// Require the game window to be focused and not minimized before accepting gameplay input or
// resuming. This prevents movement and cursor capture while the player is interacting with
// another application.
bool gameplayWindowActive() { return IsWindowFocused() && !IsWindowMinimized(); }

// Request pause when Escape is freshly pressed or the game window becomes inactive. Application
// checks this before simulation so the pause frame does not consume movement or countdown time.
bool gameplayPauseRequested() { return !gameplayWindowActive() || IsKeyPressed(KEY_ESCAPE); }

// Collect movement keys, sensitivity-scaled mouse deltas, frame duration, and fresh gravity,
// jump, and map requests into InputState. Clamp finite sensitivity and use the baseline for
// non-finite values without changing Game itself.
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

// Move a nonempty option list's selection with keyboard navigation and wrap at its ends. Opposing
// directions cancel through consecutive updates; an empty list is ignored to avoid modulo by
// zero.
void updateMenuSelection(int& selected, int count) {
    if (count <= 0) return;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % count;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + count - 1) % count;
}

// Report a fresh Enter or Space press as a menu confirmation event. Held keys do not repeatedly
// confirm because this uses IsKeyPressed rather than IsKeyDown.
bool menuConfirmed() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

// Consume queued characters accepted by the supplied filter until maxLength, then apply a pressed
// or repeated Backspace. Return whether text changed so menus can clear stale validation messages
// or update their state.
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
