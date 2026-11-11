#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace MenuTheme {
// Derive interface, map, and maze-preview color roles from one material palette. Return a
// complete value without allocating graphics resources, changing the active theme, or consuming
// random state.
Colors ColorsFor(const MazePalette& CurrentPalette) {
    Colors CurrentColors{};
    CurrentColors.CurrentPanel = ColorLerp(CurrentPalette.Trim, CurrentPalette.CurrentWall, .14f);
    CurrentColors.CurrentInk = ColorLerp(CurrentPalette.CurrentFloor, CurrentPalette.Fog, .46f);
    CurrentColors.CurrentBorder = ColorLerp(CurrentPalette.Trim, CurrentPalette.CurrentAccent, .48f);
    CurrentColors.CurrentSelected = ColorLerp(CurrentColors.CurrentPanel, CurrentPalette.CurrentAccent, .16f);
    CurrentColors.CurrentAccent = ColorLerp(CurrentPalette.CurrentAccent, CurrentPalette.CurrentPaper, .55f);
    CurrentColors.CurrentPaper = CurrentPalette.CurrentPaper;
    CurrentColors.CurrentMuted = ColorLerp(CurrentPalette.CurrentPaper, CurrentPalette.CurrentWall, .14f);
    CurrentColors.CurrentSecondary = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .90f);
    CurrentColors.CurrentDanger = ColorLerp(CurrentPalette.Lamp, CurrentPalette.CurrentPaper, .35f);
    CurrentColors.CurrentBadge = ColorLerp(CurrentColors.CurrentPanel, CurrentPalette.Trim, .15f);
    CurrentColors.CurrentKeycap = CurrentColors.CurrentSelected;
    CurrentColors.CurrentOnWorld = CurrentPalette.CurrentPaper;
    CurrentColors.CurrentMazeTop = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .55f);
    CurrentColors.CurrentMazeSage = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .20f);
    CurrentColors.CurrentMazeOlive = ColorLerp(CurrentPalette.Pattern, CurrentPalette.CurrentPaper, .35f);
    CurrentColors.CurrentMazeStone = ColorLerp(CurrentPalette.Ceiling, CurrentPalette.CurrentWall, .30f);
    CurrentColors.CurrentMazeSide = ColorLerp(CurrentPalette.Trim, CurrentPalette.CurrentWall, .30f);
    CurrentColors.CurrentMazeShade = CurrentPalette.Trim;
    CurrentColors.CurrentMazeEdge = ColorLerp(CurrentColors.CurrentMazeTop, CurrentPalette.CurrentPaper, .28f);
    CurrentColors.CurrentMazeFloor = ColorLerp(CurrentPalette.CurrentFloor, CurrentPalette.CurrentWall, .22f);
    CurrentColors.CurrentMapWall = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .50f);
    CurrentColors.CurrentMapVisited = ColorLerp(CurrentColors.CurrentPanel, CurrentColors.CurrentMapWall, .32f);
    CurrentColors.MapUnknown = ColorLerp(CurrentColors.CurrentPanel, CurrentPalette.Fog, .45f);
    CurrentColors.MapVisible = ColorLerp(CurrentColors.MapUnknown, CurrentColors.CurrentPanel, .55f);
    CurrentColors.Marker = CurrentPalette.Lamp;
    CurrentColors.CurrentTrack = ColorLerp(CurrentColors.CurrentPanel, CurrentColors.CurrentBorder, .38f);
    return CurrentColors;
}

namespace {
MazeTheme ActiveTheme = ActiveMazeTheme;
Colors ActiveColors = ColorsFor(GetMazePalette(ActiveTheme));
}

constinit const Color& Ink = ActiveColors.CurrentInk;
constinit const Color& Panel = ActiveColors.CurrentPanel;
constinit const Color& Border = ActiveColors.CurrentBorder;
constinit const Color& Selected = ActiveColors.CurrentSelected;
constinit const Color& Accent = ActiveColors.CurrentAccent;
constinit const Color& Paper = ActiveColors.CurrentPaper;
constinit const Color& Muted = ActiveColors.CurrentMuted;
constinit const Color& Secondary = ActiveColors.CurrentSecondary;
constinit const Color& Danger = ActiveColors.CurrentDanger;
constinit const Color& Badge = ActiveColors.CurrentBadge;
constinit const Color& Keycap = ActiveColors.CurrentKeycap;
constinit const Color& OnWorld = ActiveColors.CurrentOnWorld;
constinit const Color& MazeTop = ActiveColors.CurrentMazeTop;
constinit const Color& MazeSage = ActiveColors.CurrentMazeSage;
constinit const Color& MazeOlive = ActiveColors.CurrentMazeOlive;
constinit const Color& MazeStone = ActiveColors.CurrentMazeStone;
constinit const Color& MazeSide = ActiveColors.CurrentMazeSide;
constinit const Color& MazeShade = ActiveColors.CurrentMazeShade;
constinit const Color& MazeEdge = ActiveColors.CurrentMazeEdge;
constinit const Color& MazeFloor = ActiveColors.CurrentMazeFloor;
constinit const Color& MapWall = ActiveColors.CurrentMapWall;
constinit const Color& MapVisited = ActiveColors.CurrentMapVisited;

// Replace the active menu palette and its derived color-role values between frames. Exported
// const references remain bound to the same fields, so existing drawing helpers observe updated
// colors.
void Apply(MazeTheme Theme) {
    const int Index = static_cast<int>(Theme);
    ActiveTheme = Index >= 0 && Index < MazeThemeCount ? Theme : ActiveMazeTheme;
    ActiveColors = ColorsFor(GetMazePalette(ActiveTheme));
}

// Return the currently applied menu theme identifier without modifying it. Application compares
// this with ActiveMazeTheme before requesting a palette update.
MazeTheme Current() {
    return ActiveTheme;
}
}

namespace {
// Try available native font files at the requested pixel size and weight, using raylib's built-in
// font when unavailable. Custom font ownership passes to the typography cache.
Font NativeMenuFont(int Pixels, bool Bold) {
#if defined(_WIN32)
    char* Directory = nullptr;
    size_t Length = 0;
    _dupenv_s(&Directory, &Length, "WINDIR");
    const std::string Path = std::string(Directory ? Directory : "C:/Windows") + "/Fonts/" +
                             (Bold ? "segoeuib.ttf" : "segoeui.ttf");
    std::free(Directory);
    if (FileExists(Path.c_str())) {
        Font CurrentFont = LoadFontEx(Path.c_str(), Pixels, nullptr, 0);
        if (CurrentFont.texture.id && CurrentFont.texture.id != GetFontDefault().texture.id) {
            SetTextureFilter(CurrentFont.texture, TEXTURE_FILTER_BILINEAR);
            return CurrentFont;
        }
    }
#endif
    return GetFontDefault();
}
}

// Release cached custom font atlases through clear while the graphics context is still valid. The
// shared raylib default font is borrowed and is never unloaded by this owner.
MenuTypography::~MenuTypography() {
    Clear();
}

// Flush queued UI geometry before unloading cached custom font textures, then empty the cache.
// Keep the shared default font alive and allow later drawing to lazily rebuild required faces.
void MenuTypography::Clear() const {
    if (!Faces.empty())
        rlDrawRenderBatchActive();
    for (const Face& CurrentFace : Faces)
        if (CurrentFace.CurrentFont.texture.id != GetFontDefault().texture.id)
            UnloadFont(CurrentFace.CurrentFont);
    Faces.clear();
}

// Find or load a cached font for the requested pixel size and weight, clearing obsolete faces
// when UI scale changes. Flush pending drawing before texture upload so new atlases cannot
// disturb queued geometry.
Font MenuTypography::FontFor(int Pixels, bool Bold) const {
    if (CachedScale != UiScale()) {
        Clear();
        CachedScale = UiScale();
    }
    for (const Face& CurrentFace : Faces)
        if (CurrentFace.Pixels == Pixels && CurrentFace.Bold == Bold)
            return CurrentFace.CurrentFont;
    // Texture uploads must not change GPU state while earlier UI geometry is still queued.
    rlDrawRenderBatchActive();
    const Font CurrentFont = NativeMenuFont(Pixels, Bold);
    Faces.push_back({Pixels, Bold, CurrentFont});
    return CurrentFont;
}

// Fit text to its available width, select an atlas at the final pixel size, and align the
// baseline position to the pixel grid. The center flag changes horizontal anchoring while the
// shared typography cache owns custom fonts.
void MenuTypography::DrawText(const char* Value, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor, bool Bold,
                              int Width, bool CurrentCenter) const {
    if (!Value || !*Value)
        return;
    const std::string Label(Value);
    int Pixels = std::max(10, (int)std::round(CurrentSize * UiScale()));
    Font CurrentFont = FontFor(Pixels, Bold);
    float Measured = MeasureTextEx(CurrentFont, Label.c_str(), (float)Pixels, 0).x;
    const int Available = Width > 0 ? std::max(1, UiSize(Width)) : 0;
    while (Available && Measured > Available && Pixels > 1) {
        Pixels = std::max(1, std::min(Pixels - 1, (int)(Pixels * Available / Measured)));
        CurrentFont = FontFor(Pixels, Bold);
        Measured = MeasureTextEx(CurrentFont, Label.c_str(), (float)Pixels, 0).x;
    }
    const Vector2 CurrentPosition = {std::round(UiX(CurrentX) - (CurrentCenter ? Measured / 2 : 0)), (float)UiY(CurrentY)};
    DrawTextEx(CurrentFont, Label.c_str(), CurrentPosition, (float)Pixels, 0, CurrentColor);
}

// Draw left-anchored text at reference UI coordinates using optional weight and width fitting.
// Delegate measurement and font caching to drawText so all menu labels share the same
// rasterization rules.
void MenuTypography::Draw(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor, bool Bold,
                          int Width) const {
    DrawText(CurrentText, CurrentX, CurrentY, CurrentSize, CurrentColor, Bold, Width, false);
}

// Draw text around a reference center X using the same fitting and cached fonts as left-anchored
// text. The Y coordinate and nominal size use the shared reference UI scale.
void MenuTypography::Centered(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor, bool Bold,
                              int Width) const {
    DrawText(CurrentText, CurrentX, CurrentY, CurrentSize, CurrentColor, Bold, Width, true);
}

// Center the visible glyph ink inside a pixel-space badge or keycap instead of centering only the
// font's line box. This keeps short initials and key labels visually aligned in both axes.
void MenuTypography::CenteredIn(const char* Value, Rectangle CurrentBounds, int CurrentSize, Color CurrentColor,
                                bool Bold) const {
    if (!Value || !*Value || CurrentBounds.width <= 0 || CurrentBounds.height <= 0)
        return;
    const std::string Label(Value);
    int Pixels = std::max(10, (int)std::round(CurrentSize * UiScale()));
    Font CurrentFont = FontFor(Pixels, Bold);
    const float Available = std::max(1.0f, CurrentBounds.width - UiSize(12));
    float Width = MeasureTextEx(CurrentFont, Label.c_str(), (float)Pixels, 0).x;
    while (Width > Available && Pixels > 1) {
        Pixels = std::max(1, std::min(Pixels - 1, (int)(Pixels * Available / Width)));
        CurrentFont = FontFor(Pixels, Bold);
        Width = MeasureTextEx(CurrentFont, Label.c_str(), (float)Pixels, 0).x;
    }
    const float Ratio = (float)Pixels / CurrentFont.baseSize;
    float CurrentLeft = std::numeric_limits<float>::max(), Top = CurrentLeft;
    float CurrentRight = -CurrentLeft, Bottom = CurrentRight, CurrentAdvance = 0;
    for (size_t Index = 0; Index < Label.size();) {
        int Bytes = 0;
        const int Codepoint = GetCodepointNext(Label.c_str() + Index, &Bytes);
        const int Glyph = GetGlyphIndex(CurrentFont, Codepoint);
        const GlyphInfo Info = CurrentFont.glyphs[Glyph];
        const Rectangle Rect = CurrentFont.recs[Glyph];
        if (Codepoint != ' ') {
            CurrentLeft = std::min(CurrentLeft, CurrentAdvance + Info.offsetX * Ratio);
            Top = std::min(Top, Info.offsetY * Ratio);
            CurrentRight = std::max(CurrentRight, CurrentAdvance + (Info.offsetX + Rect.width) * Ratio);
            Bottom = std::max(Bottom, (Info.offsetY + Rect.height) * Ratio);
        }
        CurrentAdvance += (Info.advanceX ? Info.advanceX : Rect.width + Info.offsetX) * Ratio;
        Index += std::max(1, Bytes);
    }
    if (CurrentRight < CurrentLeft)
        return;
    const Vector2 CurrentPosition = {std::round(CurrentBounds.x + CurrentBounds.width / 2 - (CurrentLeft + CurrentRight) / 2),
                              std::round(CurrentBounds.y + CurrentBounds.height / 2 - (Top + Bottom) / 2)};
    DrawTextEx(CurrentFont, Label.c_str(), CurrentPosition, (float)Pixels, 0, CurrentColor);
}

// Choose the uniform factor that fits the 1000-by-700 reference UI inside the current window.
// Coordinate helpers center unused space rather than stretching the layout independently on each
// axis.
float UiScale() {
    return std::min(GetScreenWidth() / 1000.0f, GetScreenHeight() / 700.0f);
}

// Convert a reference-layout X coordinate into window pixels and include the horizontal centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int UiX(int CurrentX) {
    float CurrentScale = UiScale();
    return (int)((GetScreenWidth() - 1000.0f * CurrentScale) / 2.0f + CurrentX * CurrentScale);
}

// Convert a reference-layout Y coordinate into window pixels and include the vertical centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int UiY(int CurrentY) {
    float CurrentScale = UiScale();
    return (int)((GetScreenHeight() - 700.0f * CurrentScale) / 2.0f + CurrentY * CurrentScale);
}

// Scale a reference length or font size into integer pixels without adding either centering
// margin. This keeps dimensions separate from the uiX and uiY position conversions.
int UiSize(int CurrentSize) {
    return (int)(CurrentSize * UiScale());
}

// Measure the label with its actual font metrics and reduce its size only as needed to fit the
// supplied pixel width. Preserve proportional spacing, including widths that require very small
// text.
float FittedMenuFontSize(const char* CurrentText, float MaximumSize, float Width) {
    if (!CurrentText || !*CurrentText || Width <= 0 || MaximumSize <= 0)
        return 0;
    const float Measured = MeasureTextEx(GetFontDefault(), CurrentText, MaximumSize, MaximumSize / 10).x;
    return Measured > Width ? MaximumSize * Width / Measured : MaximumSize;
}

// Convert the reference Y coordinate and font size to pixels, then center the label across the
// current window. Call drawCenteredAt when those values have already been scaled.
void DrawCentered(const char* CurrentText, int CurrentY, int CurrentSize, Color CurrentColor) {
    DrawCenteredAt(CurrentText, UiY(CurrentY), UiSize(CurrentSize), CurrentColor);
}

// Draw a label horizontally centered using a Y coordinate and font size already expressed in
// window pixels. Avoid applying reference scaling a second time to callers' calculated positions.
void DrawCenteredAt(const char* CurrentText, int CurrentY, int CurrentSize, Color CurrentColor) {
    DrawText(CurrentText, (GetScreenWidth() - MeasureText(CurrentText, CurrentSize)) / 2, CurrentY, CurrentSize, CurrentColor);
}

// Draw a vertically spaced option list in reference coordinates, distinguishing the selected
// entry with its marker and theme colors. This rendering helper does not change the selected
// index.
void DrawOptions(const char* const* Labels, int CurrentCount, int CurrentSelected, int StartY) {
    for (int I = 0; I < CurrentCount; I++) {
        const char* Line = TextFormat("%s%s", I == CurrentSelected ? "> " : "  ", Labels[I]);
        DrawCentered(Line, StartY + I * 70, 40,
                     I == CurrentSelected ? MenuTheme::Accent : MenuTheme::Muted);
    }
}

// Dim the current frame and draw a centered popup with scaled dimensions, border, and fill.
// Return its pixel-space rectangle so callers can align content and controls to the actual panel.
Rectangle DrawPopup(int Width, int Height, Color CurrentBorder, Color CurrentFill, float CurrentDim) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(MenuTheme::Ink, CurrentDim));
    int CurrentW = UiSize(Width), H = UiSize(Height);
    int CurrentX = (GetScreenWidth() - CurrentW) / 2;
    int CurrentY = (GetScreenHeight() - H) / 2;
    DrawRectangleRounded({(float)CurrentX, (float)CurrentY, (float)CurrentW, (float)H}, 0.06f, 8, CurrentFill);
    DrawRectangleRoundedLinesEx({(float)CurrentX, (float)CurrentY, (float)CurrentW, (float)H}, 0.06f, 8,
                                std::max(1.0f, UiScale()), CurrentBorder);
    DrawRectangle(CurrentX + UiSize(28), CurrentY, UiSize(50), std::max(2, UiSize(3)), CurrentBorder);
    return {(float)CurrentX, (float)CurrentY, (float)CurrentW, (float)H};
}

// Draw the current input string at a centered pixel-space position and append a time-based
// blinking caret. Text editing and validation occur in the caller's update path.
void DrawTextEntry(const std::string& CurrentText, int CurrentY, int CurrentSize) {
    int Width = MeasureText(CurrentText.c_str(), CurrentSize);
    int CurrentX = (GetScreenWidth() - Width) / 2;
    DrawText(CurrentText.c_str(), CurrentX, CurrentY, CurrentSize, MenuTheme::Accent);
    if (((int)(GetTime() * 2)) % 2 == 0)
        DrawText("_", CurrentX + Width + UiSize(4), CurrentY, CurrentSize, MenuTheme::Accent);
}

// Convert a complete reference-layout rectangle into centered window-pixel coordinates. Scale
// dimensions without position offsets and transform the origin with the shared UI margins.
Rectangle UiRect(int CurrentX, int CurrentY, int Width, int Height) {
    return {(float)UiX(CurrentX), (float)UiY(CurrentY), (float)UiSize(Width), (float)UiSize(Height)};
}

// Draw themed text after scaling its reference position and font size into window pixels. This
// low-level helper leaves input handling and layout decisions to the caller.
void DrawMenuText(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor) {
    DrawText(CurrentText, UiX(CurrentX), UiY(CurrentY), std::max(1, UiSize(CurrentSize)), CurrentColor);
}

// Convert reference bounds to pixels and reduce the label size until it fits the available width.
// Use the same measurement and drawing scale so long labels do not overflow their controls.
void DrawMenuTextFit(const char* CurrentText, int CurrentX, int CurrentY, int Width, int CurrentSize, Color CurrentColor) {
    const float Pixels = FittedMenuFontSize(CurrentText, (float)UiSize(CurrentSize), (float)UiSize(Width));
    if (Pixels > 0)
        DrawTextEx(GetFontDefault(), CurrentText, {(float)UiX(CurrentX), (float)UiY(CurrentY)}, Pixels, Pixels / 10,
                   CurrentColor);
}

// Draw the shared themed backdrop treatment with the supplied menu variation. It supplies
// background decoration only; foreground navigation and the shared corridor journey are managed
// separately.
void DrawMenuBackdrop(int Variation) {
    using namespace MenuTheme;
    ClearBackground(Ink);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Panel, Ink);
    // Layer an architectural grid, corridor lines and a moving signal to create the shared animated
    // menu backdrop.
    for (int CurrentX = 24; CurrentX < 1000; CurrentX += 32)
        DrawLine(UiX(CurrentX), 0, UiX(CurrentX), GetScreenHeight(), Fade(Border, 0.14f));
    for (int CurrentY = 20; CurrentY < 700; CurrentY += 32)
        DrawLine(0, UiY(CurrentY), GetScreenWidth(), UiY(CurrentY), Fade(Border, 0.14f));
    for (int I = 0; I < 9; ++I) {
        int CurrentX = 530 + I * 30, CurrentY = 42 + I * 27;
        Color Line = Fade(Secondary, 0.045f + (I % 3) * 0.012f);
        DrawLineEx({(float)UiX(CurrentX), (float)UiY(CurrentY)}, {(float)UiX(975 - I * 12), (float)UiY(CurrentY)},
                   std::max(1.0f, UiScale()), Line);
        DrawLineEx({(float)UiX(CurrentX), (float)UiY(CurrentY)}, {(float)UiX(CurrentX), (float)UiY(605 - I * 16)},
                   std::max(1.0f, UiScale()), Line);
    }
    const Vector2 Path[] = {{640, 52},  {931, 52},  {931, 580}, {793, 580},
                            {793, 104}, {706, 104}, {706, 604}};
    float Total = 0;
    for (int I = 1; I < 7; ++I) {
        Vector2 CurrentA = {(float)UiX((int)Path[I - 1].x), (float)UiY((int)Path[I - 1].y)};
        Vector2 B = {(float)UiX((int)Path[I].x), (float)UiY((int)Path[I].y)};
        DrawLineEx(CurrentA, B, std::max(1.0f, UiScale()), Fade(Secondary, 0.10f));
        Total += std::abs(Path[I].x - Path[I - 1].x) + std::abs(Path[I].y - Path[I - 1].y);
    }
    float Distance = std::fmod((float)GetTime() * 42 + Variation * 170, Total);
    for (int I = 1; I < 7; ++I) {
        const Vector2 CurrentA = Path[I - 1], B = Path[I];
        float Length = std::abs(B.x - CurrentA.x) + std::abs(B.y - CurrentA.y);
        if (Distance <= Length) {
            float CurrentT = Distance / Length;
            Vector2 Point = {(float)UiX((int)(CurrentA.x + (B.x - CurrentA.x) * CurrentT)),
                             (float)UiY((int)(CurrentA.y + (B.y - CurrentA.y) * CurrentT))};
            DrawCircleV(Point, UiScale() * 9, Fade(Accent, 0.06f));
            DrawCircleV(Point, UiScale() * 3, Fade(Accent, 0.55f));
            break;
        }
        Distance -= Length;
    }
}

// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void DrawMenuHeading(const char* Section, const char* Title, const char* Subtitle) {
    using namespace MenuTheme;
    DrawRectangle(UiX(64), UiY(57), UiSize(24), std::max(2, UiSize(3)), Accent);
    DrawMenuText(Section, 101, 52, 14, Accent);
    DrawMenuText(Title, 61, 92, 50);
    DrawMenuText(Subtitle, 64, 154, 18, Muted);
}

// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void DrawMenuHeading(const MenuTypography& Type, const char* Section, const char* Title,
                     const char* Subtitle) {
    DrawRectangle(UiX(64), UiY(57), UiSize(24), std::max(2, UiSize(3)), MenuTheme::Accent);
    Type.Draw(Section, 101, 47, 14, MenuTheme::Accent, true);
    Type.Draw(Title, 61, 83, 52, MenuTheme::Paper, true, 740);
    Type.Draw(Subtitle, 64, 150, 17, MenuTheme::Muted, false, 800);
}

// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void DrawMenuFooter(const char* Hint) {
    DrawLine(UiX(64), UiY(624), UiX(936), UiY(624), MenuTheme::Border);
    DrawMenuText(Hint, 64, 646, 15, MenuTheme::Muted);
}

// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void DrawMenuFooter(const MenuTypography& Type, const char* Hint) {
    DrawLine(UiX(64), UiY(624), UiX(936), UiY(624), MenuTheme::Border);
    Type.Draw(Hint, 64, 643, 14, MenuTheme::Muted, false, 872);
}

// Draw a rounded themed panel and border using bounds already expressed in window pixels. The
// caller owns layout and must not pass unscaled reference coordinates.
void DrawMenuPanel(Rectangle CurrentBounds, Color CurrentBorder) {
    DrawRectangleRounded(CurrentBounds, 0.05f, 8, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(CurrentBounds, 0.05f, 8, std::max(1.0f, UiScale()), CurrentBorder);
}

// Render a selected or ordinary button inside pixel-space bounds, with a fitted label, optional
// detail line, and accent marker. Input and selection changes remain in the menu's update
// function.
void DrawMenuButton(Rectangle CurrentBounds, const char* Label, bool CurrentSelected, const char* CurrentDetail,
                    Color CurrentAccent) {
    using namespace MenuTheme;
    DrawRectangleRounded(CurrentBounds, 0.10f, 8, CurrentSelected ? Selected : Panel);
    DrawRectangleRoundedLinesEx(CurrentBounds, 0.10f, 8, std::max(1.0f, UiScale()),
                                CurrentSelected ? CurrentAccent : Border);
    if (CurrentSelected)
        DrawRectangle((int)CurrentBounds.x, (int)(CurrentBounds.y + CurrentBounds.height * 0.28f),
                      std::max(2, UiSize(3)), (int)(CurrentBounds.height * 0.44f), CurrentAccent);
    const float Available = CurrentBounds.width - UiSize(64);
    const float LabelSize = FittedMenuFontSize(Label, (float)UiSize(CurrentDetail ? 25 : 22), Available);
    if (LabelSize > 0)
        DrawTextEx(GetFontDefault(), Label,
                   {CurrentBounds.x + UiSize(22),
                    CurrentBounds.y + (CurrentDetail ? (float)UiSize(13) : (CurrentBounds.height - LabelSize) / 2)},
                   LabelSize, LabelSize / 10, CurrentSelected ? Paper : Muted);
    const float DetailSize = CurrentDetail ? FittedMenuFontSize(CurrentDetail, (float)UiSize(14), Available) : 0;
    if (DetailSize > 0)
        DrawTextEx(GetFontDefault(), CurrentDetail, {CurrentBounds.x + UiSize(22), CurrentBounds.y + UiSize(45)},
                   DetailSize, DetailSize / 10, Muted);
    DrawText(CurrentSelected ? ">" : "+", (int)(CurrentBounds.x + CurrentBounds.width) - UiSize(32),
             (int)(CurrentBounds.y + CurrentBounds.height / 2) - UiSize(9), UiSize(18),
             CurrentSelected ? CurrentAccent : Border);
}

// Set the pointer cursor when hovering a pixel-space rectangle and report only a fresh left click
// inside it. Merely hovering or holding the button does not repeatedly activate the control.
bool MenuClicked(Rectangle CurrentBounds) {
    const bool Over = CheckCollisionPointRec(GetMousePosition(), CurrentBounds);
    if (Over)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    return Over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Select a hovered option only when the pointer moves or clicks inside its pixel-space bounds. A
// stationary pointer leaves keyboard navigation intact instead of stealing the selection every
// frame.
bool MenuPointer(Rectangle CurrentBounds, int Index, int& CurrentSelected) {
    const bool Over = CheckCollisionPointRec(GetMousePosition(), CurrentBounds);
    const Vector2 Delta = GetMouseDelta();
    const bool Click = MenuClicked(CurrentBounds);
    if (Over && (Delta.x != 0 || Delta.y != 0 || Click))
        CurrentSelected = Index;
    return Click;
}

// Return the shared back button's reference rectangle transformed into window pixels. Menus reuse
// these bounds for both its rendering and pointer hit test.
Rectangle MenuBackBounds() {
    return UiRect(810, 45, 126, 36);
}
// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void DrawMenuBack() {
    const Rectangle CurrentBounds = MenuBackBounds();
    const bool Over = CheckCollisionPointRec(GetMousePosition(), CurrentBounds);
    DrawRectangleRounded(CurrentBounds, 0.08f, 6, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(CurrentBounds, 0.08f, 6, std::max(1.0f, UiScale()),
                                Over ? MenuTheme::Accent : MenuTheme::Border);
    const float Cx = CurrentBounds.x + CurrentBounds.width * .5f, CurrentCy = CurrentBounds.y + CurrentBounds.height * .5f;
    const float CurrentHalf = 13 * UiScale(), Head = 6 * UiScale();
    const float Stroke = std::max(1.5f, 2 * UiScale());
    const Color CurrentColor = Over ? MenuTheme::Accent : MenuTheme::Muted;
    const Vector2 Tip{Cx - CurrentHalf, CurrentCy};
    DrawLineEx({Cx + CurrentHalf, CurrentCy}, Tip, Stroke, CurrentColor);
    DrawLineEx({Tip.x + Head, CurrentCy - Head}, Tip, Stroke, CurrentColor);
    DrawLineEx(Tip, {Tip.x + Head, CurrentCy + Head}, Stroke, CurrentColor);
}

// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void DrawMenuBack(const MenuTypography&) {
    DrawMenuBack();
}
// Return the common entry dialog's confirmation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle EntryConfirmBounds() {
    return UiRect(318, 408, 174, 42);
}
// Return the common entry dialog's cancellation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle EntryCancelBounds() {
    return UiRect(508, 408, 174, 42);
}
// Draw the common entry dialog's confirmation and cancellation controls, emphasizing confirmation
// as the default action. The caller handles editing, validation, and the resulting button
// actions.
void DrawEntryActions() {
    DrawMenuButton(EntryConfirmBounds(), "Confirm", true);
    DrawMenuButton(EntryCancelBounds(), "Cancel", false);
}
