#pragma once
#include "../logic/Game.h"
#include <string>

InputState readInput();
void updateMenuSelection(int& selected, int count);
bool menuConfirmed();
bool editText(std::string& text, int maxLength, bool (*allowed)(int));
