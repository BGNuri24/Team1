#pragma once
#include "../logic/Game.h"
#include <string>

// Collect movement keys, sensitivity-scaled mouse deltas, frame duration, and fresh gravity,
// jump, and map requests into InputState. Clamp finite sensitivity and use the baseline for
// non-finite values without changing Game itself.
InputState readInput(float sensitivity = 1.0f);
// Require the game window to be focused and not minimized before accepting gameplay input or
// resuming. This prevents movement and cursor capture while the player is interacting with
// another application.
bool gameplayWindowActive();
// Request pause when Escape is freshly pressed or the game window becomes inactive. Application
// checks this before simulation so the pause frame does not consume movement or countdown time.
bool gameplayPauseRequested();
// Move a nonempty option list's selection with keyboard navigation and wrap at its ends. Opposing
// directions cancel through consecutive updates; an empty list is ignored to avoid modulo by
// zero.
void updateMenuSelection(int& selected, int count);
// Report a fresh Enter or Space press as a menu confirmation event. Held keys do not repeatedly
// confirm because this uses IsKeyPressed rather than IsKeyDown.
bool menuConfirmed();
// Consume queued characters accepted by the supplied filter until maxLength, then apply a pressed
// or repeated Backspace. Return whether text changed so menus can clear stale validation messages
// or update their state.
bool editText(std::string& text, int maxLength, bool (*allowed)(int));
