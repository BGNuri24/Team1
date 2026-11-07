#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace MenuTheme {
Colors colorsFor(const MazePalette& palette) {
    Colors colors{};
    colors.panel = ColorLerp(palette.floor, palette.wall, .47f);
    colors.ink = ColorLerp(palette.trim, palette.wall, .35f);
    colors.border = ColorLerp(palette.wall, palette.paper, .40f);
    colors.selected = ColorLerp(colors.panel, palette.wall, .08f);
    colors.accent = ColorLerp(palette.pattern, palette.paper, .86f);
    colors.paper = palette.paper;
    colors.muted = ColorLerp(palette.paper, palette.wall, .14f);
    colors.secondary = ColorLerp(palette.wall, palette.paper, .90f);
    colors.danger = {255, 221, 210, 255};
    colors.badge = ColorLerp(colors.panel, palette.trim, .15f);
    colors.keycap = colors.selected;
    colors.onWorld = palette.paper;
    colors.mazeTop = ColorLerp(palette.wall, palette.paper, .55f);
    colors.mazeSage = ColorLerp(palette.wall, palette.paper, .20f);
    colors.mazeOlive = ColorLerp(palette.pattern, palette.paper, .35f);
    colors.mazeStone = ColorLerp(palette.ceiling, palette.wall, .30f);
    colors.mazeSide = ColorLerp(palette.trim, palette.wall, .30f);
    colors.mazeShade = palette.trim;
    colors.mazeEdge = ColorLerp(colors.mazeTop, palette.paper, .28f);
    colors.mazeFloor = ColorLerp(palette.floor, palette.wall, .22f);
    colors.mapWall = ColorLerp(palette.wall, palette.paper, .50f);
    colors.mapVisited = ColorLerp(colors.panel, colors.mapWall, .32f);
    colors.mapUnknown = ColorLerp(colors.panel, palette.fog, .45f);
    colors.mapVisible = ColorLerp(colors.mapUnknown, colors.panel, .55f);
    colors.marker = palette.lamp;
    colors.track = ColorLerp(colors.panel, colors.border, .38f);
    return colors;
}

namespace {
MazeTheme activeTheme = MazeTheme::Slate;
Colors activeColors = colorsFor(mazePalette(activeTheme));
}

constinit const Color& Ink = activeColors.ink;
constinit const Color& Panel = activeColors.panel;
constinit const Color& Border = activeColors.border;
constinit const Color& Selected = activeColors.selected;
constinit const Color& Accent = activeColors.accent;
constinit const Color& Paper = activeColors.paper;
constinit const Color& Muted = activeColors.muted;
constinit const Color& Secondary = activeColors.secondary;
constinit const Color& Danger = activeColors.danger;
constinit const Color& Badge = activeColors.badge;
constinit const Color& Keycap = activeColors.keycap;
constinit const Color& OnWorld = activeColors.onWorld;
constinit const Color& MazeTop = activeColors.mazeTop;
constinit const Color& MazeSage = activeColors.mazeSage;
constinit const Color& MazeOlive = activeColors.mazeOlive;
constinit const Color& MazeStone = activeColors.mazeStone;
constinit const Color& MazeSide = activeColors.mazeSide;
constinit const Color& MazeShade = activeColors.mazeShade;
constinit const Color& MazeEdge = activeColors.mazeEdge;
constinit const Color& MazeFloor = activeColors.mazeFloor;
constinit const Color& MapWall = activeColors.mapWall;
constinit const Color& MapVisited = activeColors.mapVisited;

// Apply between frames. References remain bound to these fields as their values change.
void apply(MazeTheme theme) {
    const int index = static_cast<int>(theme);
    activeTheme = index >= 0 && index < MazeThemeCount ? theme : MazeTheme::Slate;
    activeColors = colorsFor(mazePalette(activeTheme));
}

MazeTheme current() {
    return activeTheme;
}
}

namespace {
// System fonts are optional; the built-in font remains available on other platforms.
Font nativeMenuFont(int pixels, bool bold) {
#if defined(_WIN32)
    char* directory = nullptr;
    size_t length = 0;
    _dupenv_s(&directory, &length, "WINDIR");
    const std::string path = std::string(directory ? directory : "C:/Windows") + "/Fonts/" +
                             (bold ? "segoeuib.ttf" : "segoeui.ttf");
    std::free(directory);
    if (FileExists(path.c_str())) {
        Font font = LoadFontEx(path.c_str(), pixels, nullptr, 0);
        if (font.texture.id && font.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            return font;
        }
    }
#endif
    return GetFontDefault();
}
}

MenuTypography::~MenuTypography() {
    clear();
}

void MenuTypography::clear() const {
    if (!faces.empty())
        rlDrawRenderBatchActive();
    for (const Face& face : faces)
        if (face.font.texture.id != GetFontDefault().texture.id)
            UnloadFont(face.font);
    faces.clear();
}

Font MenuTypography::fontFor(int pixels, bool bold) const {
    if (cachedScale != uiScale()) {
        clear();
        cachedScale = uiScale();
    }
    for (const Face& face : faces)
        if (face.pixels == pixels && face.bold == bold)
            return face.font;
    // Texture uploads must not change GPU state while earlier UI geometry is still queued.
    rlDrawRenderBatchActive();
    const Font font = nativeMenuFont(pixels, bold);
    faces.push_back({pixels, bold, font});
    return font;
}

// Measure and rasterize fitted text at its final pixel size, then align it to the pixel grid.
void MenuTypography::drawText(const char* value, int x, int y, int size, Color color, bool bold,
                              int width, bool center) const {
    if (!value || !*value)
        return;
    const std::string label(value);
    int pixels = std::max(10, (int)std::round(size * uiScale()));
    Font font = fontFor(pixels, bold);
    float measured = MeasureTextEx(font, label.c_str(), (float)pixels, 0).x;
    const int available = width > 0 ? std::max(1, uiSize(width)) : 0;
    while (available && measured > available && pixels > 1) {
        pixels = std::max(1, std::min(pixels - 1, (int)(pixels * available / measured)));
        font = fontFor(pixels, bold);
        measured = MeasureTextEx(font, label.c_str(), (float)pixels, 0).x;
    }
    const Vector2 position = {std::round(uiX(x) - (center ? measured / 2 : 0)), (float)uiY(y)};
    DrawTextEx(font, label.c_str(), position, (float)pixels, 0, color);
}

void MenuTypography::draw(const char* text, int x, int y, int size, Color color, bool bold,
                          int width) const {
    drawText(text, x, y, size, color, bold, width, false);
}

void MenuTypography::centered(const char* text, int x, int y, int size, Color color, bool bold,
                              int width) const {
    drawText(text, x, y, size, color, bold, width, true);
}

// Use the ink bounds rather than the font's ascent/line box so short initials sit at true center.
void MenuTypography::centeredIn(const char* value, Rectangle bounds, int size, Color color,
                                bool bold) const {
    if (!value || !*value || bounds.width <= 0 || bounds.height <= 0)
        return;
    const std::string label(value);
    int pixels = std::max(10, (int)std::round(size * uiScale()));
    Font font = fontFor(pixels, bold);
    const float available = std::max(1.0f, bounds.width - uiSize(12));
    float width = MeasureTextEx(font, label.c_str(), (float)pixels, 0).x;
    while (width > available && pixels > 1) {
        pixels = std::max(1, std::min(pixels - 1, (int)(pixels * available / width)));
        font = fontFor(pixels, bold);
        width = MeasureTextEx(font, label.c_str(), (float)pixels, 0).x;
    }
    const float ratio = (float)pixels / font.baseSize;
    float left = std::numeric_limits<float>::max(), top = left;
    float right = -left, bottom = right, advance = 0;
    for (size_t index = 0; index < label.size();) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(label.c_str() + index, &bytes);
        const int glyph = GetGlyphIndex(font, codepoint);
        const GlyphInfo info = font.glyphs[glyph];
        const Rectangle rect = font.recs[glyph];
        if (codepoint != ' ') {
            left = std::min(left, advance + info.offsetX * ratio);
            top = std::min(top, info.offsetY * ratio);
            right = std::max(right, advance + (info.offsetX + rect.width) * ratio);
            bottom = std::max(bottom, (info.offsetY + rect.height) * ratio);
        }
        advance += (info.advanceX ? info.advanceX : rect.width + info.offsetX) * ratio;
        index += std::max(1, bytes);
    }
    if (right < left)
        return;
    const Vector2 position = {std::round(bounds.x + bounds.width / 2 - (left + right) / 2),
                              std::round(bounds.y + bounds.height / 2 - (top + bottom) / 2)};
    DrawTextEx(font, label.c_str(), position, (float)pixels, 0, color);
}

// Choose the uniform scale that fits the 1000-by-700 reference layout inside the current window.
float uiScale() {
    return std::min(GetScreenWidth() / 1000.0f, GetScreenHeight() / 700.0f);
}

// Convert a reference-layout X coordinate to window pixels, including the horizontal centering
// offset.
int uiX(int x) {
    float scale = uiScale();
    return (int)((GetScreenWidth() - 1000.0f * scale) / 2.0f + x * scale);
}

// Convert a reference-layout Y coordinate to window pixels, including the vertical centering
// offset.
int uiY(int y) {
    float scale = uiScale();
    return (int)((GetScreenHeight() - 700.0f * scale) / 2.0f + y * scale);
}

// Scale a reference length or font size to integer pixels without adding a position offset.
int uiSize(int size) {
    return (int)(size * uiScale());
}

// Scale the actual glyph measurement rather than asking DrawText for unsupported sub-ten-pixel
// sizes.
float fittedMenuFontSize(const char* text, float maximumSize, float width) {
    if (!text || !*text || width <= 0 || maximumSize <= 0)
        return 0;
    const float measured = MeasureTextEx(GetFontDefault(), text, maximumSize, maximumSize / 10).x;
    return measured > width ? maximumSize * width / measured : maximumSize;
}

// Scale a reference Y coordinate and font size, then draw text horizontally centered in the window.
void drawCentered(const char* text, int y, int size, Color color) {
    drawCenteredAt(text, uiY(y), uiSize(size), color);
}

// Draw horizontally centered text using a Y coordinate and font size already expressed in window
// pixels.
void drawCenteredAt(const char* text, int y, int size, Color color) {
    DrawText(text, (GetScreenWidth() - MeasureText(text, size)) / 2, y, size, color);
}

// Draw a vertical option list in reference coordinates, adding a marker and highlight to the
// selected entry.
void drawOptions(const char* const* labels, int count, int selected, int startY) {
    for (int i = 0; i < count; i++) {
        const char* line = TextFormat("%s%s", i == selected ? "> " : "  ", labels[i]);
        drawCentered(line, startY + i * 70, 40,
                     i == selected ? MenuTheme::Accent : MenuTheme::Muted);
    }
}

// Dim the current frame, draw a centered scaled popup, and return its bounds in window pixels.
Rectangle drawPopup(int width, int height, Color border, Color fill, float dim) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(MenuTheme::Ink, dim));
    int w = uiSize(width), h = uiSize(height);
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    DrawRectangleRounded({(float)x, (float)y, (float)w, (float)h}, 0.06f, 8, fill);
    DrawRectangleRoundedLinesEx({(float)x, (float)y, (float)w, (float)h}, 0.06f, 8,
                                std::max(1.0f, uiScale()), border);
    DrawRectangle(x + uiSize(28), y, uiSize(50), std::max(2, uiSize(3)), border);
    return {(float)x, (float)y, (float)w, (float)h};
}

// Draw a centered input string at pixel coordinates and append a time-based blinking caret.
void drawTextEntry(const std::string& text, int y, int size) {
    int width = MeasureText(text.c_str(), size);
    int x = (GetScreenWidth() - width) / 2;
    DrawText(text.c_str(), x, y, size, MenuTheme::Accent);
    if (((int)(GetTime() * 2)) % 2 == 0)
        DrawText("_", x + width + uiSize(4), y, size, MenuTheme::Accent);
}

// Convert a complete reference-layout rectangle to centered, scaled window-pixel bounds.
Rectangle uiRect(int x, int y, int width, int height) {
    return {(float)uiX(x), (float)uiY(y), (float)uiSize(width), (float)uiSize(height)};
}

// Draw themed text after converting its reference position and font size to window pixels.
void drawMenuText(const char* text, int x, int y, int size, Color color) {
    DrawText(text, uiX(x), uiY(y), std::max(1, uiSize(size)), color);
}

// Convert reference dimensions to pixels and reduce the pixel font size while the measured label
// exceeds the available width.
void drawMenuTextFit(const char* text, int x, int y, int width, int size, Color color) {
    const float pixels = fittedMenuFontSize(text, (float)uiSize(size), (float)uiSize(width));
    if (pixels > 0)
        DrawTextEx(GetFontDefault(), text, {(float)uiX(x), (float)uiY(y)}, pixels, pixels / 10,
                   color);
}

// Render the shared gradient, architectural grid and animated corridor signal, offset by the chosen
// menu variation.
void drawMenuBackdrop(int variation) {
    using namespace MenuTheme;
    ClearBackground(Ink);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Panel, Ink);
    // Layer an architectural grid, corridor lines and a moving signal to create the shared animated
    // menu backdrop.
    for (int x = 24; x < 1000; x += 32)
        DrawLine(uiX(x), 0, uiX(x), GetScreenHeight(), Fade(Border, 0.14f));
    for (int y = 20; y < 700; y += 32)
        DrawLine(0, uiY(y), GetScreenWidth(), uiY(y), Fade(Border, 0.14f));
    for (int i = 0; i < 9; ++i) {
        int x = 530 + i * 30, y = 42 + i * 27;
        Color line = Fade(Secondary, 0.045f + (i % 3) * 0.012f);
        DrawLineEx({(float)uiX(x), (float)uiY(y)}, {(float)uiX(975 - i * 12), (float)uiY(y)},
                   std::max(1.0f, uiScale()), line);
        DrawLineEx({(float)uiX(x), (float)uiY(y)}, {(float)uiX(x), (float)uiY(605 - i * 16)},
                   std::max(1.0f, uiScale()), line);
    }
    const Vector2 path[] = {{640, 52},  {931, 52},  {931, 580}, {793, 580},
                            {793, 104}, {706, 104}, {706, 604}};
    float total = 0;
    for (int i = 1; i < 7; ++i) {
        Vector2 a = {(float)uiX((int)path[i - 1].x), (float)uiY((int)path[i - 1].y)};
        Vector2 b = {(float)uiX((int)path[i].x), (float)uiY((int)path[i].y)};
        DrawLineEx(a, b, std::max(1.0f, uiScale()), Fade(Secondary, 0.10f));
        total += std::abs(path[i].x - path[i - 1].x) + std::abs(path[i].y - path[i - 1].y);
    }
    float distance = std::fmod((float)GetTime() * 42 + variation * 170, total);
    for (int i = 1; i < 7; ++i) {
        const Vector2 a = path[i - 1], b = path[i];
        float length = std::abs(b.x - a.x) + std::abs(b.y - a.y);
        if (distance <= length) {
            float t = distance / length;
            Vector2 point = {(float)uiX((int)(a.x + (b.x - a.x) * t)),
                             (float)uiY((int)(a.y + (b.y - a.y) * t))};
            DrawCircleV(point, uiScale() * 9, Fade(Accent, 0.06f));
            DrawCircleV(point, uiScale() * 3, Fade(Accent, 0.55f));
            break;
        }
        distance -= length;
    }
}

// Draw the section marker, title and subtitle at the common reference-layout heading positions.
void drawMenuHeading(const char* section, const char* title, const char* subtitle) {
    using namespace MenuTheme;
    DrawRectangle(uiX(64), uiY(57), uiSize(24), std::max(2, uiSize(3)), Accent);
    drawMenuText(section, 101, 52, 14, Accent);
    drawMenuText(title, 61, 92, 50);
    drawMenuText(subtitle, 64, 154, 18, Muted);
}

void drawMenuHeading(const MenuTypography& type, const char* section, const char* title,
                     const char* subtitle) {
    DrawRectangle(uiX(64), uiY(57), uiSize(24), std::max(2, uiSize(3)), MenuTheme::Accent);
    type.draw(section, 101, 47, 14, MenuTheme::Accent, true);
    type.draw(title, 61, 83, 52, MenuTheme::Paper, true, 740);
    type.draw(subtitle, 64, 150, 17, MenuTheme::Muted, false, 800);
}

// Draw the shared footer divider and its navigation or gameplay hint.
void drawMenuFooter(const char* hint) {
    DrawLine(uiX(64), uiY(624), uiX(936), uiY(624), MenuTheme::Border);
    drawMenuText(hint, 64, 646, 15, MenuTheme::Muted);
}

void drawMenuFooter(const MenuTypography& type, const char* hint) {
    DrawLine(uiX(64), uiY(624), uiX(936), uiY(624), MenuTheme::Border);
    type.draw(hint, 64, 643, 14, MenuTheme::Muted, false, 872);
}

// Draw a rounded theme panel and border using bounds that are already expressed in window pixels.
void drawMenuPanel(Rectangle bounds, Color border) {
    DrawRectangleRounded(bounds, 0.05f, 8, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(bounds, 0.05f, 8, std::max(1.0f, uiScale()), border);
}

// Render a selectable themed button with an optional detail line, fitted label and accent indicator
// inside pixel-space bounds.
void drawMenuButton(Rectangle bounds, const char* label, bool selected, const char* detail,
                    Color accent) {
    using namespace MenuTheme;
    DrawRectangleRounded(bounds, 0.10f, 8, selected ? Selected : Panel);
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()),
                                selected ? accent : Border);
    if (selected)
        DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height * 0.28f),
                      std::max(2, uiSize(3)), (int)(bounds.height * 0.44f), accent);
    const float available = bounds.width - uiSize(64);
    const float labelSize = fittedMenuFontSize(label, (float)uiSize(detail ? 25 : 22), available);
    if (labelSize > 0)
        DrawTextEx(GetFontDefault(), label,
                   {bounds.x + uiSize(22),
                    bounds.y + (detail ? (float)uiSize(13) : (bounds.height - labelSize) / 2)},
                   labelSize, labelSize / 10, selected ? Paper : Muted);
    const float detailSize = detail ? fittedMenuFontSize(detail, (float)uiSize(14), available) : 0;
    if (detailSize > 0)
        DrawTextEx(GetFontDefault(), detail, {bounds.x + uiSize(22), bounds.y + uiSize(45)},
                   detailSize, detailSize / 10, Muted);
    DrawText(selected ? ">" : "+", (int)(bounds.x + bounds.width) - uiSize(32),
             (int)(bounds.y + bounds.height / 2) - uiSize(9), uiSize(18),
             selected ? accent : Border);
}

// Set the hover cursor for a pixel-space rectangle and return true only for a fresh left click
// inside it.
bool menuClicked(Rectangle bounds) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (over)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    return over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Select the hovered option only when the pointer moves or clicks, preserving keyboard focus
// beneath a stationary cursor.
bool menuPointer(Rectangle bounds, int index, int& selected) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    const Vector2 delta = GetMouseDelta();
    const bool click = menuClicked(bounds);
    if (over && (delta.x != 0 || delta.y != 0 || click))
        selected = index;
    return click;
}

// Return the common back button's reference rectangle converted to window pixels.
Rectangle menuBackBounds() {
    return uiRect(810, 45, 126, 36);
}
// Render the common back button using its live hover state.
void drawMenuBack() {
    drawMenuButton(menuBackBounds(), "Back",
                   CheckCollisionPointRec(GetMousePosition(), menuBackBounds()));
}

void drawMenuBack(const MenuTypography& type) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), menuBackBounds());
    DrawRectangleRounded(menuBackBounds(), 0.08f, 6, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(menuBackBounds(), 0.08f, 6, std::max(1.0f, uiScale()),
                                over ? MenuTheme::Accent : MenuTheme::Border);
    type.centered("BACK", 873, 52, 14, over ? MenuTheme::Accent : MenuTheme::Muted, true);
}
// Return the shared entry dialog's confirmation-button hit rectangle in window pixels.
Rectangle entryConfirmBounds() {
    return uiRect(318, 408, 174, 42);
}
// Return the shared entry dialog's cancel-button hit rectangle in window pixels.
Rectangle entryCancelBounds() {
    return uiRect(508, 408, 174, 42);
}
// Render the shared confirmation and cancellation buttons with confirmation highlighted as the
// default action.
void drawEntryActions() {
    drawMenuButton(entryConfirmBounds(), "Confirm", true);
    drawMenuButton(entryCancelBounds(), "Cancel", false);
}
