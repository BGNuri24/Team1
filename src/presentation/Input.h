#pragma once
#include "../logic/Game.h"
#include <string>

// Read one frame of input and scale only the mouse delta; sensitivity 1.0 keeps the original look speed.
InputState readInput(float sensitivity = 1.0f);
// Check focus and minimization before capturing input or resuming a paused run.
bool gameplayWindowActive();
// Request a pause before simulation advances when Escape is pressed or the window loses focus.
bool gameplayPauseRequested();
void updateMenuSelection(int& selected, int count);
bool menuConfirmed();
bool editText(std::string& text, int maxLength, bool (*allowed)(int));
