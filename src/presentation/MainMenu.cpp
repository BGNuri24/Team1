#include "MainMenu.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace {
constexpr int COUNT = 3;
const char* const LABELS[COUNT] = {"PLAY", "CONTROLS", "QUIT"};
constexpr Screen DESTINATIONS[COUNT] = {Screen::PLAYER_SELECT, Screen::CONTROLS, Screen::QUIT};
const Color& ACCENT = MenuTheme::Accent;
const Color& TEXT = MenuTheme::Paper;
const Color& MUTED = MenuTheme::Muted;

// Keep the home screen's wide composition centered and usable down to the minimum window size.
float scale() {
    return std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
}

Vector2 point(float x, float y) {
    const float s = scale();
    return {(GetScreenWidth() - 1280 * s) * 0.5f + x * s,
            (GetScreenHeight() - 720 * s) * 0.5f + y * s};
}

float fontSize(float size) {
    return std::max(12.0f, std::round(size * scale()));
}

Rectangle bounds(float x, float y, float width, float height) {
    const Vector2 p = point(x, y);
    return {p.x, p.y, width * scale(), height * scale()};
}

Rectangle optionBounds(int index) {
    return bounds(85, 250 + index * 65.0f, 258, 58);
}

// Borrow the default font when system typography is unavailable; no downloaded assets are needed.
Font loadMenuFont(const char* filename, int size) {
#if defined(_WIN32)
    char* directory = nullptr;
    size_t length = 0;
    _dupenv_s(&directory, &length, "WINDIR");
    const std::string path =
        std::string(directory ? directory : "C:/Windows") + "/Fonts/" + filename;
    std::free(directory);
    if (FileExists(path.c_str())) {
        Font font = LoadFontEx(path.c_str(), size, nullptr, 0);
        if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            return font;
        }
    }
#endif
    return GetFontDefault();
}

void releaseFont(Font& font) {
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id)
        UnloadFont(font);
    font = {};
}

// Snap native-size glyphs to whole pixels instead of resampling a larger font atlas.
void text(Font font, const char* label, float x, float y, float size, float spacing, Color color) {
    Vector2 position = point(x, y);
    position = {std::round(position.x), std::round(position.y)};
    DrawTextEx(font, label, position, fontSize(size), std::round(spacing * scale()), color);
}

void centeredText(Font font, const char* label, float x, float y, float size, float spacing,
                  Color color) {
    const float pixels = fontSize(size);
    const float tracking = std::round(spacing * scale());
    const float width = MeasureTextEx(font, label, pixels, tracking).x;
    Vector2 position = point(x, y);
    position = {std::round(position.x - width * 0.5f), std::round(position.y)};
    DrawTextEx(font, label, position, pixels, tracking, color);
}

void line(float x1, float y1, float x2, float y2, Color color, float width = 1) {
    DrawLineEx(point(x1, y1), point(x2, y2), std::max(1.0f, width * scale()), color);
}

void diamond(float x, float y, float radius, Color color) {
    DrawTriangle(point(x, y - radius), point(x - radius, y), point(x, y + radius), color);
    DrawTriangle(point(x, y - radius), point(x, y + radius), point(x + radius, y), color);
}

// Draw the open labyrinth motif above the wordmark.
void mazeMark(float x, float y, float size, Color color) {
    const float u = size / 6;
    line(x - 3 * u, y + 3 * u, x - 3 * u, y - 3 * u, color, 1.5f);
    line(x - 3 * u, y - 3 * u, x + 3 * u, y - 3 * u, color, 1.5f);
    line(x + 3 * u, y - 3 * u, x + 3 * u, y + 3 * u, color, 1.5f);
    line(x + 3 * u, y + 3 * u, x - u, y + 3 * u, color, 1.5f);
    line(x - u, y + 3 * u, x - u, y - u, color, 1.5f);
    line(x - u, y - u, x + u, y - u, color, 1.5f);
    line(x + u, y - u, x + u, y + u, color, 1.5f);
}

}

// Own optional system fonts inside the graphics context, alongside the animated background.
MainMenu::MainMenu() : selected(0) {
    refreshFonts();
}

MainMenu::~MainMenu() {
    releaseFont(titleFont);
    releaseFont(menuFont);
    releaseFont(detailFont);
}

// Rasterize each font at its actual display size, including after resizing the window.
void MainMenu::refreshFonts() const {
    const std::array<int, 3> pixels = {(int)fontSize(96), (int)fontSize(26), (int)fontSize(14)};
    if (fontPixels == pixels)
        return;
    releaseFont(titleFont);
    releaseFont(menuFont);
    releaseFont(detailFont);
    titleFont = loadMenuFont("timesbd.ttf", pixels[0]);
    menuFont = loadMenuFont("segoeuib.ttf", pixels[1]);
    detailFont = loadMenuFont("segoeui.ttf", pixels[2]);
    fontPixels = pixels;
}

// Preserve keyboard selection under a stationary mouse and use the drawn bounds for all clicks.
Screen MainMenu::update() {
    updateMenuSelection(selected, COUNT);
    const Vector2 mouse = GetMousePosition();
    const Vector2 delta = GetMouseDelta();
    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool hovered = false;
    for (int index = 0; index < COUNT; ++index) {
        if (!CheckCollisionPointRec(mouse, optionBounds(index)))
            continue;
        hovered = true;
        if (delta.x != 0 || delta.y != 0 || clicked)
            selected = index;
        if (clicked) {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return DESTINATIONS[index];
        }
    }
    SetMouseCursor(hovered ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    if (menuConfirmed()) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return DESTINATIONS[selected];
    }
    return Screen::MAIN_MENU;
}

// Float the navigation over the living maze, leaving most of the world visible.
void MainMenu::draw() const {
    refreshFonts();
    const float time = (float)GetTime();
    background.draw(time);
    const float s = scale();

    // A fine inset frame and quiet corner details hold the composition together at any aspect
    // ratio.
    DrawRectangleLinesEx({12 * s, 12 * s, GetScreenWidth() - 24 * s, GetScreenHeight() - 24 * s},
                         std::max(1.0f, s), MenuTheme::Border);
    line(38, 39, 84, 39, MenuTheme::Border);
    line(38, 39, 38, 68, MenuTheme::Border);
    line(1242, 652, 1242, 681, MenuTheme::Border);
    line(1196, 681, 1242, 681, MenuTheme::Border);

    mazeMark(217, 70, 23, ACCENT);
    line(155, 70, 191, 70, MenuTheme::Border);
    line(243, 70, 279, 70, MenuTheme::Border);
    centeredText(titleFont, "T1", 216, 96, 96, 9, TEXT);
    line(92, 211, 201, 211, MenuTheme::Border);
    diamond(216, 211, 3, ACCENT);
    line(231, 211, 340, 211, MenuTheme::Border);

    for (int index = 0; index < COUNT; ++index) {
        const bool active = index == selected;
        const float y = 250 + index * 65.0f;
        if (active) {
            const Rectangle area = optionBounds(index);
            DrawRectangleRec(area, MenuTheme::Selected);
            line(85, y + 10, 85, y + 48, ACCENT, 2);
            diamond(96, y + 27, 4, ACCENT);
        } else {
            diamond(96, y + 27, 2, MenuTheme::Border);
        }
        text(menuFont, LABELS[index], 122, y + 10, 26, 2, active ? TEXT : MUTED);
    }

    line(84, 635, 336, 635, MenuTheme::Border);
    text(detailFont, "UP / DOWN   navigate     ENTER   select", 84, 650, 14, 0, MUTED);
}
