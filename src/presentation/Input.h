#pragma once
#include "../logic/Game.h"
#include <string>

// Read one frame of input and scale only the mouse delta; sensitivity 1.0 keeps the original look speed.
InputState readInput(float sensitivity = 1.0f);
void updateMenuSelection(int& selected, int count);
bool menuConfirmed();
bool editText(std::string& text, int maxLength, bool (*allowed)(int));
