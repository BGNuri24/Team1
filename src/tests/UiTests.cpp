#include "../presentation/Ui.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
void runResourceTests();

namespace {
// Use a hidden graphics context so measurements match the default font used by real menu drawing.
struct Window {
    Window() {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(640, 480, "Menu text checks");
    }
    ~Window() { CloseWindow(); }
};

// Assert actual rendered-font metrics, including widths that require a font size below ten pixels.
void checkFit(const char* text, float maximum, float width) {
    const float size = fittedMenuFontSize(text, maximum, width);
    if (!(size > 0 && size <= maximum))
        throw std::runtime_error("fitted font must be positive and bounded");
    const float measured = MeasureTextEx(GetFontDefault(), text, size, size / 10).x;
    if (measured > width + .001f)
        throw std::runtime_error("fitted glyphs overflow their available width");
}

bool sameColor(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

float luminance(Color color) {
    auto channel = [](unsigned char value) {
        const float x = value / 255.0f;
        return x <= .04045f ? x / 12.92f : std::pow((x + .055f) / 1.055f, 2.4f);
    };
    return .2126f * channel(color.r) + .7152f * channel(color.g) + .0722f * channel(color.b);
}

// Exercise a single live UI across theme switches, checking rendered panels, default text colors
// and readable normal/selected states rather than reconstructing the palette formulas in the test.
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
    if (MenuTheme::current() != MazeTheme::Slate)
        throw std::runtime_error("unsupported UI themes must fall back to Slate");
    MenuTheme::apply(saved);
}
}

// Verify proportional font fitting and export a small-window preview without taking desktop focus.
void runUiTests() {
    Window window;
    if (!IsWindowReady())
        throw std::runtime_error("hidden UI test context unavailable");
    runResourceTests();
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
