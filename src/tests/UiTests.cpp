#include "../presentation/Ui.h"
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
}

// Verify proportional font fitting and export a small-window preview without taking desktop focus.
void runUiTests() {
    Window window;
    if (!IsWindowReady())
        throw std::runtime_error("hidden UI test context unavailable");
    runResourceTests();
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
