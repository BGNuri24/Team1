#include "Ui.h"
#include <algorithm>
#include <cmath>

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
    DrawRectangleRounded({ (float)x, (float)y, (float)w, (float)h }, 0.06f, 8, fill);
    DrawRectangleRoundedLinesEx({ (float)x, (float)y, (float)w, (float)h },
        0.06f, 8, std::max(1.0f, uiScale()), border);
    DrawRectangle(x + uiSize(28), y, uiSize(50), std::max(2, uiSize(3)), border);
    return { (float)x, (float)y, (float)w, (float)h };
}

void drawTextEntry(const std::string& text, int y, int size) {
    int width = MeasureText(text.c_str(), size);
    int x = (GetScreenWidth() - width) / 2;
    DrawText(text.c_str(), x, y, size, MenuTheme::Amber);
    if (((int)(GetTime() * 2)) % 2 == 0)
        DrawText("_", x + width + uiSize(4), y, size, MenuTheme::Amber);
}

Rectangle uiRect(int x, int y, int width, int height) {
    return { (float)uiX(x), (float)uiY(y), (float)uiSize(width), (float)uiSize(height) };
}

void drawMenuText(const char* text, int x, int y, int size, Color color) {
    DrawText(text, uiX(x), uiY(y), std::max(1, uiSize(size)), color);
}

void drawMenuTextFit(const char* text, int x, int y, int width, int size, Color color) {
    int pixels = std::max(1, uiSize(size));
    while (pixels > 1 && MeasureText(text, pixels) > uiSize(width)) --pixels;
    DrawText(text, uiX(x), uiY(y), pixels, color);
}

void drawMenuBackdrop(int variation) {
    using namespace MenuTheme;
    ClearBackground(Ink);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
        Color{ 16, 35, 42, 255 }, Ink);
    // Architectural grid, open corridors and a slow travelling signal.
    for (int x = 24; x < 1000; x += 32)
        DrawLine(uiX(x), 0, uiX(x), GetScreenHeight(), Fade(Border, 0.14f));
    for (int y = 20; y < 700; y += 32)
        DrawLine(0, uiY(y), GetScreenWidth(), uiY(y), Fade(Border, 0.14f));
    for (int i = 0; i < 9; ++i) {
        int x = 530 + i * 30, y = 42 + i * 27;
        Color line = Fade(Mint, 0.045f + (i % 3) * 0.012f);
        DrawLineEx({ (float)uiX(x), (float)uiY(y) },
            { (float)uiX(975 - i * 12), (float)uiY(y) }, std::max(1.0f, uiScale()), line);
        DrawLineEx({ (float)uiX(x), (float)uiY(y) },
            { (float)uiX(x), (float)uiY(605 - i * 16) }, std::max(1.0f, uiScale()), line);
    }
    const Vector2 path[] = { {640, 52}, {931, 52}, {931, 580}, {793, 580},
                            {793, 104}, {706, 104}, {706, 604} };
    float total = 0;
    for (int i = 1; i < 7; ++i) {
        Vector2 a = { (float)uiX((int)path[i - 1].x), (float)uiY((int)path[i - 1].y) };
        Vector2 b = { (float)uiX((int)path[i].x), (float)uiY((int)path[i].y) };
        DrawLineEx(a, b, std::max(1.0f, uiScale()), Fade(Mint, 0.10f));
        total += std::abs(path[i].x - path[i - 1].x) + std::abs(path[i].y - path[i - 1].y);
    }
    float distance = std::fmod((float)GetTime() * 42 + variation * 170, total);
    for (int i = 1; i < 7; ++i) {
        const Vector2 a = path[i - 1], b = path[i];
        float length = std::abs(b.x - a.x) + std::abs(b.y - a.y);
        if (distance <= length) {
            float t = distance / length;
            Vector2 point = { (float)uiX((int)(a.x + (b.x - a.x) * t)),
                              (float)uiY((int)(a.y + (b.y - a.y) * t)) };
            DrawCircleV(point, uiScale() * 9, Fade(Amber, 0.06f));
            DrawCircleV(point, uiScale() * 3, Fade(Amber, 0.55f));
            break;
        }
        distance -= length;
    }
}

void drawMenuHeading(const char* section, const char* title, const char* subtitle) {
    using namespace MenuTheme;
    DrawRectangle(uiX(64), uiY(57), uiSize(24), std::max(2, uiSize(3)), Amber);
    drawMenuText(section, 101, 52, 14, Amber);
    drawMenuText(title, 61, 92, 50);
    drawMenuText(subtitle, 64, 154, 18, Muted);
}

void drawMenuFooter(const char* hint) {
    DrawLine(uiX(64), uiY(624), uiX(936), uiY(624), MenuTheme::Border);
    drawMenuText(hint, 64, 646, 15, MenuTheme::Muted);
}

void drawMenuPanel(Rectangle bounds, Color border) {
    DrawRectangleRounded(bounds, 0.05f, 8, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(bounds, 0.05f, 8, std::max(1.0f, uiScale()), border);
}

void drawMenuButton(Rectangle bounds, const char* label, bool selected,
    const char* detail, Color accent) {
    using namespace MenuTheme;
    DrawRectangleRounded(bounds, 0.10f, 8,
        selected ? Color{ 36, 47, 46, 255 } : Color{ 16, 33, 40, 245 });
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()),
        selected ? Fade(accent, 0.8f) : Border);
    if (selected)
        DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height * 0.28f),
            std::max(2, uiSize(3)), (int)(bounds.height * 0.44f), accent);
    int labelSize = uiSize(detail ? 25 : 22);
    while (labelSize > 1 && MeasureText(label, labelSize) > bounds.width - uiSize(64)) --labelSize;
    DrawText(label, (int)bounds.x + uiSize(22),
        (int)bounds.y + (detail ? uiSize(13) : (int)(bounds.height - labelSize) / 2),
        labelSize, selected ? Paper : Muted);
    if (detail)
        DrawText(detail, (int)bounds.x + uiSize(22), (int)bounds.y + uiSize(45), uiSize(14), Muted);
    DrawText(selected ? ">" : "+", (int)(bounds.x + bounds.width) - uiSize(32),
        (int)(bounds.y + bounds.height / 2) - uiSize(9), uiSize(18), selected ? accent : Border);
}

bool menuClicked(Rectangle bounds) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (over) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

bool menuPointer(Rectangle bounds, int index, int& selected) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    const Vector2 delta = GetMouseDelta();
    const bool click = menuClicked(bounds);
    if (over && (delta.x != 0 || delta.y != 0 || click)) selected = index;
    return click;
}

Rectangle menuBackBounds() { return uiRect(810, 45, 126, 36); }
void drawMenuBack() {
    drawMenuButton(menuBackBounds(), "Back", CheckCollisionPointRec(GetMousePosition(), menuBackBounds()));
}
Rectangle entryConfirmBounds() { return uiRect(318, 408, 174, 42); }
Rectangle entryCancelBounds() { return uiRect(508, 408, 174, 42); }
void drawEntryActions() {
    drawMenuButton(entryConfirmBounds(), "Confirm", true);
    drawMenuButton(entryCancelBounds(), "Cancel", false);
}
