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
    drawCenteredAt(text, uiY(y), uiSize(size), color);
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

Rectangle drawPopup(int width, int height, Color border, Color fill, float dim) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, dim));
    int w = uiSize(width), h = uiSize(height);
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    DrawRectangle(x, y, w, h, fill);
    DrawRectangleLines(x, y, w, h, border);
    return {(float)x, (float)y, (float)w, (float)h};
}

void drawTextEntry(const std::string& text, int y, int size) {
    int width = MeasureText(text.c_str(), size);
    int x = (GetScreenWidth() - width) / 2;
    DrawText(text.c_str(), x, y, size, GOLD);
    if (((int)(GetTime() * 2)) % 2 == 0)
        DrawText("_", x + width + uiSize(4), y, size, GOLD);
}
