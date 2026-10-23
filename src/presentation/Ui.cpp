#include "Ui.h"
#include <algorithm>

float uiScale() {
    return std::min(GetScreenWidth() / 1000.0f, GetScreenHeight() / 700.0f);
}

int uiX(int x) {
    float scale = uiScale();
    return (int)((GetScreenWidth() - 1000.0f * scale) / 2.0f + x * scale);
}

int uiY(int y) {
    float scale = uiScale();
    return (int)((GetScreenHeight() - 700.0f * scale) / 2.0f + y * scale);
}

int uiSize(int size) {
    return (int)(size * uiScale());
}

void drawCentered(const char* text, int y, int size, Color color) {
    int scaledSize = uiSize(size);
    DrawText(text, (GetScreenWidth() - MeasureText(text, scaledSize)) / 2,
             uiY(y), scaledSize, color);
}

void drawCenteredAt(const char* text, int y, int size, Color color) {
    DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, y, size, color);
}

void drawOptions(const char* const* labels, int count, int selected, int startY) {
    for (int i = 0; i < count; i++) {
        const char* line = TextFormat("%s%s", i == selected ? "> " : "  ", labels[i]);
        drawCentered(line, startY + i * 70, 40, i == selected ? GOLD : GRAY);
    }
}