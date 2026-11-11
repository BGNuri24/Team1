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
constexpr float TitleStretch = 1.65f;
const char* const LABELS[COUNT] = {"PLAY", "CONTROLS", "QUIT"};
constexpr Screen DESTINATIONS[COUNT] = {Screen::PlayerSelect, Screen::CONTROLS, Screen::QUIT};
const Color& ACCENT = MenuTheme::Accent;
const Color& TEXT = MenuTheme::Paper;
const Color& MUTED = MenuTheme::Muted;

// Fit the main menu's 1280-by-720 reference layout into the current window while preserving its
// aspect ratio. Other helpers use this common factor for positions, typography, and ornament
// widths.
float Scale() {
    return std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
}

// Convert a reference-layout point into window pixels, centering the scaled 1280-by-720 canvas.
// This keeps menu artwork and hit regions aligned when the window aspect ratio changes.
Vector2 Point(float CurrentX, float CurrentY) {
    const float CurrentS = Scale();
    return {(GetScreenWidth() - 1280 * CurrentS) * 0.5f + CurrentX * CurrentS,
            (GetScreenHeight() - 720 * CurrentS) * 0.5f + CurrentY * CurrentS};
}

// Scale a reference font size to a rounded pixel size with a minimum of 12 pixels. Rebuilding
// fonts at this size keeps text crisp instead of stretching a single small atlas.
float FontSize(float CurrentSize) {
    return std::max(12.0f, std::round(CurrentSize * Scale()));
}

// Convert a reference rectangle's origin through point and scale its dimensions. The same result
// is used for menu hit testing and drawing so visual and interactive regions match.
Rectangle Bounds(float CurrentX, float CurrentY, float Width, float Height) {
    const Vector2 P = Point(CurrentX, CurrentY);
    return {P.x, P.y, Width * Scale(), Height * Scale()};
}

// Return the scaled rectangle for a zero-based main-menu option. Options share a fixed reference
// width and vertical spacing, keeping pointer selection aligned with their labels.
Rectangle OptionBounds(int Index) {
    return Bounds(440, 335 + Index * 76.0f, 400, 62);
}

// Load an available font at the requested pixel size and optional glyph set, applying bilinear
// filtering to a successful custom atlas. Return raylib's default font when the file is missing
// or loading fails.
Font LoadFontFile(const std::string& Path, int CurrentSize, int* Glyphs = nullptr, int CurrentCount = 0) {
    if (FileExists(Path.c_str())) {
        Font CurrentFont = LoadFontEx(Path.c_str(), CurrentSize, Glyphs, CurrentCount);
        if (CurrentFont.texture.id != 0 && CurrentFont.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(CurrentFont.texture, TEXTURE_FILTER_BILINEAR);
            return CurrentFont;
        }
    }
    return GetFontDefault();
}

// Load the main menu's supporting typeface using its available font paths and requested pixel
// size. Keep a default-font fallback so missing system fonts do not prevent menu rendering.
Font LoadMenuFont(const char* Filename, int CurrentSize, int* Glyphs = nullptr, int CurrentCount = 0) {
#if defined(_WIN32)
    char* Directory = nullptr;
    size_t Length = 0;
    _dupenv_s(&Directory, &Length, "WINDIR");
    const std::string Path =
        std::string(Directory ? Directory : "C:/Windows") + "/Fonts/" + Filename;
    std::free(Directory);
    return LoadFontFile(Path, CurrentSize, Glyphs, CurrentCount);
#endif
    return GetFontDefault();
}

// Load the bundled title face at the requested size, including the glyphs required by the
// wordmark. Development and packaged asset paths are checked before falling back to an available
// menu font.
Font LoadTitleFont(int CurrentSize) {
    int Glyphs[] = {'T', '1'};
    const std::string Packaged = std::string(GetApplicationDirectory()) + "assets/fonts/Ballet.ttf";
    for (const std::string& Path : {Packaged, std::string("src/assets/fonts/Ballet.ttf")}) {
        Font CurrentFont = LoadFontFile(Path, CurrentSize, Glyphs, 2);
        if (CurrentFont.texture.id != GetFontDefault().texture.id)
            return CurrentFont;
    }
    return LoadMenuFont("Gabriola.ttf", CurrentSize, Glyphs, 2);
}

// Unload a custom font atlas when present and clear its handle. Never unload raylib's shared
// default font, which can be returned by a failed custom-font load.
void ReleaseFont(Font& CurrentFont) {
    if (CurrentFont.texture.id != 0 && CurrentFont.texture.id != GetFontDefault().texture.id)
        UnloadFont(CurrentFont);
    CurrentFont = {};
}

// Measure a label with its scaled pixel size and letter spacing, then draw it around a reference
// center X. Round its final position so the font atlas stays aligned to screen pixels.
void CenteredText(Font CurrentFont, const char* Label, float CurrentX, float CurrentY, float CurrentSize, float Spacing,
                  Color CurrentColor) {
    const float Pixels = FontSize(CurrentSize);
    const float Tracking = std::round(Spacing * Scale());
    const float Width = MeasureTextEx(CurrentFont, Label, Pixels, Tracking).x;
    Vector2 CurrentPosition = Point(CurrentX, CurrentY);
    CurrentPosition = {std::round(CurrentPosition.x - Width * 0.5f), std::round(CurrentPosition.y)};
    DrawTextEx(CurrentFont, Label, CurrentPosition, Pixels, Tracking, CurrentColor);
}

// Draw an ornament line between two reference-layout points, scaling its thickness with the
// window. Enforce a one-pixel minimum so small-window decorations remain visible.
void Line(float X1, float Y1, float X2, float Y2, Color CurrentColor, float Width = 1) {
    DrawLineEx(Point(X1, Y1), Point(X2, Y2), std::max(1.0f, Width * Scale()), CurrentColor);
}

// Draw a filled diamond from two triangles around a reference point. Position and radius follow
// the same scaled canvas as menu text and dividers.
void Diamond(float CurrentX, float CurrentY, float Radius, Color CurrentColor) {
    DrawTriangle(Point(CurrentX, CurrentY - Radius), Point(CurrentX - Radius, CurrentY), Point(CurrentX, CurrentY + Radius), CurrentColor);
    DrawTriangle(Point(CurrentX, CurrentY - Radius), Point(CurrentX, CurrentY + Radius), Point(CurrentX + Radius, CurrentY), CurrentColor);
}

// Measure the visible ink bounds of the T1 glyphs and fit the title into its reference area with
// the intended horizontal stretch. Center the actual glyph artwork rather than its font line box
// before drawing.
void Wordmark(Font CurrentFont, Color CurrentColor) {
    float CurrentLeft = std::numeric_limits<float>::max(), Top = CurrentLeft;
    float CurrentRight = -CurrentLeft, Bottom = CurrentRight, CurrentAdvance = 0;
    for (int Codepoint : {'T', '1'}) {
        const int Index = GetGlyphIndex(CurrentFont, Codepoint);
        const GlyphInfo& Glyph = CurrentFont.glyphs[Index];
        const Rectangle& Rect = CurrentFont.recs[Index];
        CurrentLeft = std::min(CurrentLeft, CurrentAdvance + Glyph.offsetX);
        Top = std::min(Top, (float)Glyph.offsetY);
        CurrentRight = std::max(CurrentRight, CurrentAdvance + Glyph.offsetX + Rect.width);
        Bottom = std::max(Bottom, Glyph.offsetY + Rect.height);
        CurrentAdvance += Glyph.advanceX ? Glyph.advanceX : Rect.width + Glyph.offsetX;
    }
    const float CurrentFit = std::min(150 * Scale() / std::max(1.0f, Bottom - Top),
                               510 * Scale() / std::max(1.0f, (CurrentRight - CurrentLeft) * TitleStretch));
    const Vector2 Anchor = Point(640, 190);
    rlPushMatrix();
    rlTranslatef(std::round(Anchor.x), std::round(Anchor.y), 0);
    rlScalef(CurrentFit * TitleStretch, CurrentFit, 1);
    DrawTextEx(CurrentFont, "T1", {-(CurrentLeft + CurrentRight) * .5f, -(Top + Bottom) * .5f}, (float)CurrentFont.baseSize, 0,
               CurrentColor);
    rlPopMatrix();
}

} // namespace

// Initialize the main-menu selection and font cache, loading typefaces for the current window
// scale. The menu owns its custom font handles and releases them in its destructor.
MainMenu::MainMenu() : CurrentSelected(0) {
    RefreshFonts();
}

// Release the title, option, and detail font handles while leaving raylib's shared default font
// intact. Font cleanup must happen before the application closes the graphics context.
MainMenu::~MainMenu() {
    ReleaseFont(TitleFont);
    ReleaseFont(MenuFont);
    ReleaseFont(DetailFont);
}

// Rebuild title and supporting font atlases when the effective menu scale changes. Reuse cached
// fonts at an unchanged size and release old custom atlases before replacement.
void MainMenu::RefreshFonts() const {
    const std::array<int, 3> Pixels = {(int)std::ceil(FontSize(180) * TitleStretch),
                                       (int)FontSize(24), (int)FontSize(14)};
    if (FontPixels == Pixels)
        return;
    ReleaseFont(TitleFont);
    ReleaseFont(MenuFont);
    ReleaseFont(DetailFont);
    TitleFont = LoadTitleFont(Pixels[0]);
    MenuFont = LoadMenuFont("segoeuib.ttf", Pixels[1]);
    DetailFont = LoadMenuFont("segoeui.ttf", Pixels[2]);
    FontPixels = Pixels;
}

// Process keyboard and pointer navigation for the main options and return a requested application
// screen. Maintain selection between frames without starting a game or changing shared
// application ownership directly.
Screen MainMenu::Update() {
    // Translate the selected main-menu option into its destination screen. Keyboard confirmation
    // and pointer activation share this decision so they cannot choose different routes.
    auto CurrentChoose = [&] {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return DESTINATIONS[CurrentSelected];
    };
    UpdateMenuSelection(CurrentSelected, COUNT);
    const Vector2 CurrentMouse = GetMousePosition();
    const Vector2 Delta = GetMouseDelta();
    const bool Clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool Hovered = false;
    for (int Index = 0; Index < COUNT; ++Index) {
        if (!CheckCollisionPointRec(CurrentMouse, OptionBounds(Index)))
            continue;
        Hovered = true;
        if (Delta.x != 0 || Delta.y != 0 || Clicked)
            CurrentSelected = Index;
        if (Clicked) {
            return CurrentChoose();
        }
    }
    SetMouseCursor(Hovered ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    if (MenuConfirmed()) {
        return CurrentChoose();
    }
    return Screen::MainMenu;
}

// Render the main-menu foreground, wordmark, selectable options, and supporting decorations over
// the shared corridor. Refresh scale-dependent fonts as needed while leaving navigation decisions
// to update.
void MainMenu::Draw() const {
    RefreshFonts();
    const float CurrentTime = (float)GetTime();
    Background.Draw(CurrentTime);
    const float CurrentS = Scale();
    Wordmark(TitleFont, TEXT);
    Line(474, 280, 610, 280, Fade(ACCENT, .70f));
    Diamond(640, 280, 4, ACCENT);
    Line(670, 280, 806, 280, Fade(ACCENT, .70f));

    for (int Index = 0; Index < COUNT; ++Index) {
        const bool Active = Index == CurrentSelected;
        const float CurrentY = 335 + Index * 76.0f;
        const Rectangle Area = OptionBounds(Index);
        DrawRectangleRounded(
            Area, .06f, 6,
            Fade(Active ? MenuTheme::Selected : MenuTheme::Panel, Active ? .91f : .78f));
        DrawRectangleRoundedLinesEx(Area, .06f, 6, std::max(1.0f, CurrentS),
                                    Active ? ACCENT : MenuTheme::Border);
        CenteredText(MenuFont, LABELS[Index], 640, CurrentY + 15, 24, 3, Active ? TEXT : MUTED);
        if (Active) {
            Diamond(466, CurrentY + 31, 3, ACCENT);
            Diamond(814, CurrentY + 31, 3, ACCENT);
        }
    }

    CenteredText(DetailFont, "UP / DOWN   navigate     ENTER   select", 640, 650, 14, 0,
                 Fade(TEXT, .80f));
}
