#pragma once

#include "raylib.h"
#include <string>

float uiScale();
int uiX(int x);
int uiY(int y);
int uiSize(int size);

void drawCentered(const char* text, int y, int size, Color color);
void drawCenteredAt(const char* text, int y, int size, Color color);
void drawOptions(const char* const* labels, int count, int selected, int startY);
Rectangle drawPopup(int width, int height, Color border = GOLD,
                    Color fill = {25, 28, 38, 255}, float dim = 0.7f);
void drawTextEntry(const std::string& text, int y, int size);
