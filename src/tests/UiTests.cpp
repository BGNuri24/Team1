#include "../presentation/Ui.h"
#include <array>
#include <cmath>
#include <filesystem>
#include <stdexcept>
// Exercise exclusive texture ownership, move construction, move assignment, and replacement of an
// already loaded set inside a live graphics context. Verify moved-from objects become empty and
// cleanup does not double-unload handles.
void RunResourceTests();
// Run lighting, batching, HDR, bloom, material, and surface-effect checks inside the UI suite's
// hidden graphics context. Inspect rendered results without updating gameplay or writing player
// saves.
void RunLightingTests();
// Check journey behavior, scaled option targets, keyboard-pointer interaction, and packaged menu
// artwork in the hidden UI graphics context. Synthetic input avoids changing desktop focus or
// sending real clicks.
void RunMainMenuTests();

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
void CheckFit(const char* CurrentText, float Maximum, float Width) {
    const float CurrentSize = FittedMenuFontSize(CurrentText, Maximum, Width);
    if (!(CurrentSize > 0 && CurrentSize <= Maximum))
        throw std::runtime_error("fitted font must be positive and bounded");
    const float Measured = MeasureTextEx(GetFontDefault(), CurrentText, CurrentSize, CurrentSize / 10).x;
    if (Measured > Width + .001f)
        throw std::runtime_error("fitted glyphs overflow their available width");
}

// Compare all four byte channels of two colors exactly. Palette tests use this for values that
// should remain identical rather than relying on approximate brightness comparisons.
bool SameColor(Color CurrentA, Color B) {
    return CurrentA.r == B.r && CurrentA.g == B.g && CurrentA.b == B.b && CurrentA.a == B.a;
}

// Convert sRGB color channels into linear light and combine them with standard luminance weights.
// Contrast checks use this perceptual brightness estimate instead of averaging encoded RGB bytes.
float Luminance(Color CurrentColor) {
    // Decode an sRGB byte into linear-light intensity using the piecewise transfer curve.
    // Luminance comparisons must combine linear channels rather than encoded display values.
    auto Channel = [](unsigned char Value) {
        const float CurrentX = Value / 255.0f;
        return CurrentX <= .04045f ? CurrentX / 12.92f : std::pow((CurrentX + .055f) / 1.055f, 2.4f);
    };
    return .2126f * Channel(CurrentColor.r) + .7152f * Channel(CurrentColor.g) + .0722f * Channel(CurrentColor.b);
}

// Exercise a single live UI across theme switches, checking rendered panels, default text colors
// and readable normal/selected states rather than reconstructing the palette formulas in the
// test.
void CheckRunPalettes() {
    const MazeTheme Saved = MenuTheme::Current();
    const Color& LivePanel = MenuTheme::Panel;
    std::array<Color, MazeThemeCount> Backgrounds{};
    RenderTexture2D Canvas = LoadRenderTexture(80, 80);
    for (int Index = 0; Index < MazeThemeCount; ++Index) {
        const MazeTheme Theme = static_cast<MazeTheme>(Index);
        MenuTheme::Apply(Theme);
        const MenuTheme::Colors CurrentColors = MenuTheme::ColorsFor(GetMazePalette(Theme));
        if (MenuTheme::Current() != Theme || !SameColor(LivePanel, CurrentColors.CurrentPanel))
            throw std::runtime_error("existing screens must read the newly applied theme");
        Backgrounds[Index] = MenuTheme::Ink;
        for (int Previous = 0; Previous < Index; ++Previous)
            if (SameColor(Backgrounds[Previous], Backgrounds[Index]))
                throw std::runtime_error("each maze theme must produce a distinct UI background");
        for (Color Foreground :
             {CurrentColors.CurrentPaper, CurrentColors.CurrentMuted, CurrentColors.CurrentAccent, CurrentColors.CurrentSecondary, CurrentColors.CurrentDanger})
            for (Color Background :
                 {CurrentColors.CurrentInk, CurrentColors.CurrentPanel, CurrentColors.CurrentSelected, CurrentColors.CurrentBadge, CurrentColors.CurrentKeycap}) {
                const float CurrentA = Luminance(Foreground), B = Luminance(Background);
                const float CurrentContrast = (std::max(CurrentA, B) + .05f) / (std::min(CurrentA, B) + .05f);
                if (CurrentContrast < 4.5f)
                    throw std::runtime_error(std::string(GetMazePalette(Theme).CurrentName) +
                                             " UI text contrast is too low");
            }
        BeginDrawing();
        BeginTextureMode(Canvas);
        ClearBackground(MenuTheme::Ink);
        DrawMenuPanel({8, 8, 64, 64});
        DrawMenuText("X", 16, 16, 20);
        EndTextureMode();
        EndDrawing();
        Image Rendered = LoadImageFromTexture(Canvas.texture);
        ImageFlipVertical(&Rendered);
        if (!SameColor(GetImageColor(Rendered, 40, 40), CurrentColors.CurrentPanel))
            throw std::runtime_error("rendered panel retained an earlier theme");
        bool FoundText = false;
        for (int CurrentY = 20; CurrentY < 40; ++CurrentY)
            for (int CurrentX = 8; CurrentX < 32; ++CurrentX)
                FoundText |= SameColor(GetImageColor(Rendered, CurrentX, CurrentY), CurrentColors.CurrentPaper);
        UnloadImage(Rendered);
        if (!FoundText)
            throw std::runtime_error("default text color retained an earlier theme");
    }
    UnloadRenderTexture(Canvas);
    MenuTheme::Apply(static_cast<MazeTheme>(-1));
    if (MenuTheme::Current() != ActiveMazeTheme)
        throw std::runtime_error("unsupported UI themes must fall back to the active palette");
    MenuTheme::Apply(Saved);
}
}

// Create the hidden graphics context and run font-fitting, palette, resource, menu, and lighting
// checks. Export the small-window preview within the test output while keeping real gameplay and
// saves untouched.
void RunUiTests() {
    Window CurrentWindow;
    if (!IsWindowReady())
        throw std::runtime_error("hidden UI test context unavailable");
    RunResourceTests();
    RunLightingTests();
    RunMainMenuTests();
    CheckRunPalettes();
    for (float Width : {12.0f, 50.0f, 120.0f, 300.0f}) {
        CheckFit("A deliberately long menu label with several words", 25, Width);
        CheckFit("mrtakethatri", 22, Width);
        CheckFit("Change player", 16, Width);
    }
    if (FittedMenuFontSize("text", 20, 0) != 0 || FittedMenuFontSize("", 20, 100) != 0 ||
        FittedMenuFontSize(nullptr, 20, 100) != 0)
        throw std::runtime_error("empty text bounds must skip drawing");
    if (!(FittedMenuFontSize("A very long label", 20, 20) < 10))
        throw std::runtime_error(
            "fitting must support font sizes below DrawText's ten-pixel clamp");
    RenderTexture2D Preview = LoadRenderTexture(640, 480);
    BeginTextureMode(Preview);
    ClearBackground(MenuTheme::Ink);
    DrawMenuHeading("TEXT FIT / SMALL WINDOW", "Menu sizing check",
                    "Long labels should remain inside their panels.");
    DrawMenuButton(UiRect(64, 250, 414, 66), "Change player", true,
                   "Keyboard and mouse navigation");
    DrawMenuButton(UiRect(64, 335, 414, 66), "A deliberately long menu label with several words",
                   false);
    DrawMenuTextFit("VeryLongPlayerNameForCheckingBounds", 550, 270, 350, 30, MenuTheme::Accent);
    EndTextureMode();
    Image CurrentImage = LoadImageFromTexture(Preview.texture);
    ImageFlipVertical(&CurrentImage);
    std::filesystem::create_directories("docs/assets");
    ExportImage(CurrentImage, "docs/assets/UiTextSmall.png");
    UnloadImage(CurrentImage);
    UnloadRenderTexture(Preview);
}
