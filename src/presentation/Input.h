#pragma once
#include "../logic/Game.h"
#include <string>

// Sensitivity scales mouse look only; 1.0 preserves the original camera speed.
InputState readInput(float sensitivity = 1.0f);
void updateMenuSelection(int& selected, int count);
bool menuConfirmed();
bool editText(std::string& text, int maxLength, bool (*allowed)(int));
