#pragma once

#include "raylib.h";


float uiScale();
int uiX(int x);
int uiY(int y);
int uiSize(int size);

void drawCentered(char* textm int ym int size, Color color);

void drawCentered(const char* text, int y, int size, Color color);

void drawOptions(const char* const* labels, int count, int selected, int startY);