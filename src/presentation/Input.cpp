#include "Input.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

// Require the game window to be focused and not minimized before accepting gameplay input or
// resuming. This prevents movement and cursor capture while the player is interacting with
// another application.
bool GameplayWindowActive() { return IsWindowFocused() && !IsWindowMinimized(); }

// Request pause when Escape is freshly pressed or the game window becomes inactive. Application
// checks this before simulation so the pause frame does not consume movement or countdown time.
bool GameplayPauseRequested() { return !GameplayWindowActive() || IsKeyPressed(KEY_ESCAPE); }

// Collect movement keys, sensitivity-scaled mouse deltas, frame duration, and fresh gravity,
// jump, and map requests into InputState. Clamp finite sensitivity and use the baseline for
// non-finite values without changing Game itself.
InputState ReadInput(float CurrentSensitivity) {
    InputState In;
    In.CurrentForward = (float)(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        - (float)(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    In.CurrentRight = (float)(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        - (float)(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    Vector2 Md = GetMouseDelta();
    const float Multiplier = std::isfinite(CurrentSensitivity)
        ? std::clamp(CurrentSensitivity, 0.25f, 3.0f) : 1.0f;
    In.MouseDx = Md.x * Multiplier;
    In.MouseDy = Md.y * Multiplier;
    In.Dt = GetFrameTime();
    In.GravityToggle = IsKeyPressed(KEY_G);
    In.JumpPressed = IsKeyPressed(KEY_SPACE) || GetMouseWheelMove() != 0;
    In.MapPressed = IsKeyPressed(KEY_E);
    return In;
}

// Move a nonempty option list's selection with keyboard navigation and wrap at its ends. Opposing
// directions cancel through consecutive updates; an empty list is ignored to avoid modulo by
// zero.
void UpdateMenuSelection(int& CurrentSelected, int CurrentCount) {
    if (CurrentCount <= 0) return;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) CurrentSelected = (CurrentSelected + 1) % CurrentCount;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) CurrentSelected = (CurrentSelected + CurrentCount - 1) % CurrentCount;
}

// Report a fresh Enter or Space press as a menu confirmation event. Held keys do not repeatedly
// confirm because this uses IsKeyPressed rather than IsKeyDown.
bool MenuConfirmed() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

// Consume queued characters accepted by the supplied filter until maxLength, then apply a pressed
// or repeated Backspace. Return whether text changed so menus can clear stale validation messages
// or update their state.
bool EditText(std::string& CurrentText, int MaxLength, bool (*Allowed)(int)) {
    bool Changed = false;
    for (int Ch = GetCharPressed(); Ch > 0; Ch = GetCharPressed()) {
        if (Allowed(Ch) && (int)CurrentText.size() < MaxLength) {
            CurrentText += (char)Ch;
            Changed = true;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !CurrentText.empty()) {
        CurrentText.pop_back();
        Changed = true;
    }
    return Changed;
}
