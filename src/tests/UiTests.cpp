#include "../presentation/Ui.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
// Exercise exclusive texture ownership, move construction, move assignment, and replacement of an
// already loaded set inside a live graphics context. Verify moved-from objects become empty and
// cleanup does not double-unload handles.
void runResourceTests();
// Run lighting, batching, HDR, bloom, material, and surface-effect checks inside the UI suite's
// hidden graphics context. Inspect rendered results without updating gameplay or writing player
// saves.
void runLightingTests();
// Check journey behavior, scaled option targets, keyboard-pointer interaction, and packaged menu
// artwork in the hidden UI graphics context. Synthetic input avoids changing desktop focus or
// sending real clicks.
void runMainMenuTests();

namespace {
// Use a hidden graphics context so measurements match the default font used by real menu drawing.
struct Window {
    // Create a hidden 640-by-480 raylib window and graphics context with warning-level logging.
    // GPU, typography, and resource tests can run without taking desktop focus.
    Window() {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(640, 480, "Menu text checks");
    }
    // Close the hidden test window after scoped graphics resources have been released. This ends
    // the UI suite's graphics context without affecting the user's game window.
    ~Window() { CloseWindow(); }
};

// Measure text using the actual drawing font and assert its fitted size respects the available
// pixel width. Include very narrow labels to cover sizes below the usual ten-pixel drawing
// baseline.
void checkFit(const char* text, float maximum, float width) {
    const float size = fittedMenuFontSize(text, maximum, width);
    if (!(size > 0 && size <= maximum))
        throw std::runtime_error("fitted font must be positive and bounded");
    const float measured = MeasureTextEx(GetFontDefault(), text, size, size / 10).x;
    if (measured > width + .001f)
        throw std::runtime_error("fitted glyphs overflow their available width");
}

// Compare all four byte channels of two colors exactly. Palette tests use this for values that
// should remain identical rather than relying on approximate brightness comparisons.
bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// Convert sRGB color channels into linear light and combine them with standard luminance weights.
// Contrast checks use this perceptual brightness estimate instead of averaging encoded RGB bytes.
float luminance(Color color) {
    // Decode an sRGB byte into linear-light intensity using the piecewise transfer curve.
    // Luminance comparisons must combine linear channels rather than encoded display values.
    auto channel = [](unsigned char value) {
        const float x = value / 255.0f;
        return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
    };
    return .2126f * channel(color.r) + .7152f * channel(color.g) + .0722f * channel(color.b);
}

// Exercise a single live UI across theme switches, checking rendered panels, default text colors
// and readable normal/selected states rather than reconstructing the palette formulas in the
// test.
void checkRunPalettes() {
    const MazeTheme saved = MenuTheme::current();
    const Color& livePanel = MenuTheme::Panel;
    std::array<Color, MazeThemeCount> backgrounds{};
    RenderTexture2D canvas = LoadRenderTexture(80, 80);
    for (int index = 0; index < MazeThemeCount; ++index) {
        const MazeTheme theme = static_cast<MazeTheme>(index);
        MenuTheme::apply(theme);
        const MenuTheme::Colors colors = MenuTheme::colorsFor(mazePalette(theme));
        if (MenuTheme::current() != theme || !sameColor(livePanel, colors.panel))
            throw std::runtime_error("existing screens must read the newly applied theme");
        backgrounds[index] = MenuTheme::Ink;
        for (int previous = 0; previous < index; ++previous)
            if (sameColor(backgrounds[previous], backgrounds[index]))
                throw std::runtime_error("each maze theme must produce a distinct UI background");
        for (Color foreground :
             {colors.paper, colors.muted, colors.accent, colors.secondary, colors.danger})
            for (Color background :
                 {colors.ink, colors.panel, colors.selected, colors.badge, colors.keycap}) {
                const float a = luminance(foreground), b = luminance(background);
                const float contrast = (std::max(a, b) + .05f) / (std::min(a, b) + .05f);
                if (contrast < 4.5f)
                    throw std::runtime_error(std::string(mazePalette(theme).name) +
                                             " UI text contrast is too low");
            }
        BeginDrawing();
        BeginTextureMode(canvas);
        ClearBackground(MenuTheme::Ink);
        drawMenuPanel({8, 8, 64, 64});
        drawMenuText("X", 16, 16, 20);
        EndTextureMode();
        EndDrawing();
        Image rendered = LoadImageFromTexture(canvas.texture);
        ImageFlipVertical(&rendered);
        if (!sameColor(GetImageColor(rendered, 40, 40), colors.panel))
            throw std::runtime_error("rendered panel retained an earlier theme");
        bool foundText = false;
        for (int y = 20; y < 40; ++y)
            for (int x = 8; x < 32; ++x)
                foundText |= sameColor(GetImageColor(rendered, x, y), colors.paper);
        UnloadImage(rendered);
        if (!foundText)
            throw std::runtime_error("default text color retained an earlier theme");
    }
    UnloadRenderTexture(canvas);
    MenuTheme::apply(static_cast<MazeTheme>(-1));
    if (MenuTheme::current() != ActiveMazeTheme)
        throw std::runtime_error("unsupported UI themes must fall back to the active palette");
    MenuTheme::apply(saved);
}
}

// Create the hidden graphics context and run font-fitting, palette, resource, menu, and lighting
// checks. Export the small-window preview within the test output while keeping real gameplay and
// saves untouched.
void runUiTests() {
    Window window;
    if (!IsWindowReady())
        throw std::runtime_error("hidden UI test context unavailable");
    runResourceTests();
    runLightingTests();
    runMainMenuTests();
    checkRunPalettes();
    for (float width : {12.0f, 50.0f, 120.0f, 300.0f}) {
        checkFit("A deliberately long menu label with several words", 25, width);
        checkFit("mrtakethatri", 22, width);
        checkFit("Change player", 16, width);
    }
    if (fittedMenuFontSize("text", 20, 0) != 0 || fittedMenuFontSize("", 20, 100) != 0 ||
        fittedMenuFontSize(nullptr, 20, 100) != 0)
        throw std::runtime_error("empty text bounds must skip drawing");
    if (!(fittedMenuFontSize("A very long label", 20, 20) < 10))
        throw std::runtime_error(
            "fitting must support font sizes below DrawText's ten-pixel clamp");
    RenderTexture2D preview = LoadRenderTexture(640, 480);
    BeginTextureMode(preview);
    ClearBackground(MenuTheme::Ink);
    drawMenuHeading("TEXT FIT / SMALL WINDOW", "Menu sizing check",
                    "Long labels should remain inside their panels.");
    drawMenuButton(uiRect(64, 250, 414, 66), "Change player", true,
                   "Keyboard and mouse navigation");
    drawMenuButton(uiRect(64, 335, 414, 66), "A deliberately long menu label with several words",
                   false);
    drawMenuTextFit("VeryLongPlayerNameForCheckingBounds", 550, 270, 350, 30, MenuTheme::Accent);
    EndTextureMode();
    Image image = LoadImageFromTexture(preview.texture);
    ImageFlipVertical(&image);
    std::filesystem::create_directories("docs/assets");
    ExportImage(image, "docs/assets/ui-text-small.png");
    UnloadImage(image);
    UnloadRenderTexture(preview);
}
