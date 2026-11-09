#include "MainMenu.h"
#include "Input.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

namespace {
constexpr int COUNT = 3;
constexpr float TITLE_STRETCH = 1.65f;
const char* const LABELS[COUNT] = {"PLAY", "CONTROLS", "QUIT"};
constexpr Screen DESTINATIONS[COUNT] = {Screen::PLAYER_SELECT, Screen::CONTROLS, Screen::QUIT};
const Color& ACCENT = MenuTheme::Accent;
const Color& TEXT = MenuTheme::Paper;
const Color& MUTED = MenuTheme::Muted;

// Fit the main menu's 1280-by-720 reference layout into the current window while preserving its
// aspect ratio. Other helpers use this common factor for positions, typography, and ornament
// widths.
float scale() {
    return std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
}

// Convert a reference-layout point into window pixels, centering the scaled 1280-by-720 canvas.
// This keeps menu artwork and hit regions aligned when the window aspect ratio changes.
Vector2 point(float x, float y) {
    const float s = scale();
    return {(GetScreenWidth() - 1280 * s) * 0.5f + x * s,
            (GetScreenHeight() - 720 * s) * 0.5f + y * s};
}

// Scale a reference font size to a rounded pixel size with a minimum of 12 pixels. Rebuilding
// fonts at this size keeps text crisp instead of stretching a single small atlas.
float fontSize(float size) {
    return std::max(12.0f, std::round(size * scale()));
}

// Convert a reference rectangle's origin through point and scale its dimensions. The same result
// is used for menu hit testing and drawing so visual and interactive regions match.
Rectangle bounds(float x, float y, float width, float height) {
    const Vector2 p = point(x, y);
    return {p.x, p.y, width * scale(), height * scale()};
}

// Return the scaled rectangle for a zero-based main-menu option. Options share a fixed reference
// width and vertical spacing, keeping pointer selection aligned with their labels.
Rectangle optionBounds(int index) {
    return bounds(440, 335 + index * 76.0f, 400, 62);
}

// Load an available font at the requested pixel size and optional glyph set, applying bilinear
// filtering to a successful custom atlas. Return raylib's default font when the file is missing
// or loading fails.
Font loadFontFile(const std::string& path, int size, int* glyphs = nullptr, int count = 0) {
    if (FileExists(path.c_str())) {
        Font font = LoadFontEx(path.c_str(), size, glyphs, count);
        if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            return font;
        }
    }
    return GetFontDefault();
}

// Load the main menu's supporting typeface using its available font paths and requested pixel
// size. Keep a default-font fallback so missing system fonts do not prevent menu rendering.
Font loadMenuFont(const char* filename, int size, int* glyphs = nullptr, int count = 0) {
#if defined(_WIN32)
    char* directory = nullptr;
    size_t length = 0;
    _dupenv_s(&directory, &length, "WINDIR");
    const std::string path =
        std::string(directory ? directory : "C:/Windows") + "/Fonts/" + filename;
    std::free(directory);
    return loadFontFile(path, size, glyphs, count);
#endif
    return GetFontDefault();
}

// Load the bundled title face at the requested size, including the glyphs required by the
// wordmark. Development and packaged asset paths are checked before falling back to an available
// menu font.
Font loadTitleFont(int size) {
    int glyphs[] = {'T', '1'};
    const std::string packaged = std::string(GetApplicationDirectory()) + "assets/fonts/Ballet.ttf";
    for (const std::string& path : {packaged, std::string("src/assets/fonts/Ballet.ttf")}) {
        Font font = loadFontFile(path, size, glyphs, 2);
        if (font.texture.id != GetFontDefault().texture.id)
            return font;
    }
    return loadMenuFont("Gabriola.ttf", size, glyphs, 2);
}

// Unload a custom font atlas when present and clear its handle. Never unload raylib's shared
// default font, which can be returned by a failed custom-font load.
void releaseFont(Font& font) {
    if (font.texture.id != 0 && font.texture.id != GetFontDefault().texture.id)
        UnloadFont(font);
    font = {};
}

// Measure a label with its scaled pixel size and letter spacing, then draw it around a reference
// center X. Round its final position so the font atlas stays aligned to screen pixels.
void centeredText(Font font, const char* label, float x, float y, float size, float spacing,
                  Color color) {
    const float pixels = fontSize(size);
    const float tracking = std::round(spacing * scale());
    const float width = MeasureTextEx(font, label, pixels, tracking).x;
    Vector2 position = point(x, y);
    position = {std::round(position.x - width * 0.5f), std::round(position.y)};
    DrawTextEx(font, label, position, pixels, tracking, color);
}

// Draw an ornament line between two reference-layout points, scaling its thickness with the
// window. Enforce a one-pixel minimum so small-window decorations remain visible.
void line(float x1, float y1, float x2, float y2, Color color, float width = 1) {
    DrawLineEx(point(x1, y1), point(x2, y2), std::max(1.0f, width * scale()), color);
}

// Draw a filled diamond from two triangles around a reference point. Position and radius follow
// the same scaled canvas as menu text and dividers.
void diamond(float x, float y, float radius, Color color) {
    DrawTriangle(point(x, y - radius), point(x - radius, y), point(x, y + radius), color);
    DrawTriangle(point(x, y - radius), point(x, y + radius), point(x + radius, y), color);
}

// Measure the visible ink bounds of the T1 glyphs and fit the title into its reference area with
// the intended horizontal stretch. Center the actual glyph artwork rather than its font line box
// before drawing.
void wordmark(Font font, Color color) {
    float left = std::numeric_limits<float>::max(), top = left;
    float right = -left, bottom = right, advance = 0;
    for (int codepoint : {'T', '1'}) {
        const int index = GetGlyphIndex(font, codepoint);
        const GlyphInfo& glyph = font.glyphs[index];
        const Rectangle& rect = font.recs[index];
        left = std::min(left, advance + glyph.offsetX);
        top = std::min(top, (float)glyph.offsetY);
        right = std::max(right, advance + glyph.offsetX + rect.width);
        bottom = std::max(bottom, glyph.offsetY + rect.height);
        advance += glyph.advanceX ? glyph.advanceX : rect.width + glyph.offsetX;
    }
    const float fit = std::min(150 * scale() / std::max(1.0f, bottom - top),
                               510 * scale() / std::max(1.0f, (right - left) * TITLE_STRETCH));
    const Vector2 anchor = point(640, 190);
    rlPushMatrix();
    rlTranslatef(std::round(anchor.x), std::round(anchor.y), 0);
    rlScalef(fit * TITLE_STRETCH, fit, 1);
    DrawTextEx(font, "T1", {-(left + right) * .5f, -(top + bottom) * .5f}, (float)font.baseSize, 0,
               color);
    rlPopMatrix();
}

} // namespace

// Initialize the main-menu selection and font cache, loading typefaces for the current window
// scale. The menu owns its custom font handles and releases them in its destructor.
MainMenu::MainMenu() : selected(0) {
    refreshFonts();
}

// Release the title, option, and detail font handles while leaving raylib's shared default font
// intact. Font cleanup must happen before the application closes the graphics context.
MainMenu::~MainMenu() {
    releaseFont(titleFont);
    releaseFont(menuFont);
    releaseFont(detailFont);
}

// Rebuild title and supporting font atlases when the effective menu scale changes. Reuse cached
// fonts at an unchanged size and release old custom atlases before replacement.
void MainMenu::refreshFonts() const {
    const std::array<int, 3> pixels = {(int)std::ceil(fontSize(180) * TITLE_STRETCH),
                                       (int)fontSize(24), (int)fontSize(14)};
    if (fontPixels == pixels)
        return;
    releaseFont(titleFont);
    releaseFont(menuFont);
    releaseFont(detailFont);
    titleFont = loadTitleFont(pixels[0]);
    menuFont = loadMenuFont("segoeuib.ttf", pixels[1]);
    detailFont = loadMenuFont("segoeui.ttf", pixels[2]);
    fontPixels = pixels;
}

// Process keyboard and pointer navigation for the main options and return a requested application
// screen. Maintain selection between frames without starting a game or changing shared
// application ownership directly.
Screen MainMenu::update() {
    // Translate the selected main-menu option into its destination screen. Keyboard confirmation
    // and pointer activation share this decision so they cannot choose different routes.
    auto choose = [&] {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return DESTINATIONS[selected];
    };
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
            return choose();
        }
    }
    SetMouseCursor(hovered ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    if (menuConfirmed()) {
        return choose();
    }
    return Screen::MAIN_MENU;
}

// Render the main-menu foreground, wordmark, selectable options, and supporting decorations over
// the shared corridor. Refresh scale-dependent fonts as needed while leaving navigation decisions
// to update.
void MainMenu::draw() const {
    refreshFonts();
    const float time = (float)GetTime();
    background.draw(time);
    const float s = scale();
    wordmark(titleFont, TEXT);
    line(474, 280, 610, 280, Fade(ACCENT, .70f));
    diamond(640, 280, 4, ACCENT);
    line(670, 280, 806, 280, Fade(ACCENT, .70f));

    for (int index = 0; index < COUNT; ++index) {
        const bool active = index == selected;
        const float y = 335 + index * 76.0f;
        const Rectangle area = optionBounds(index);
        DrawRectangleRounded(
            area, .06f, 6,
            Fade(active ? MenuTheme::Selected : MenuTheme::Panel, active ? .91f : .78f));
        DrawRectangleRoundedLinesEx(area, .06f, 6, std::max(1.0f, s),
                                    active ? ACCENT : MenuTheme::Border);
        centeredText(menuFont, LABELS[index], 640, y + 15, 24, 3, active ? TEXT : MUTED);
        if (active) {
            diamond(466, y + 31, 3, ACCENT);
            diamond(814, y + 31, 3, ACCENT);
        }
    }

    centeredText(detailFont, "UP / DOWN   navigate     ENTER   select", 640, 650, 14, 0,
                 Fade(TEXT, .80f));
}
