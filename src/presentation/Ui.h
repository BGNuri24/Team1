#pragma once

#include "MazePalette.h"
#include "raylib.h"
#include <string>
#include <vector>

// Choose the uniform factor that fits the 1000-by-700 reference UI inside the current window.
// Coordinate helpers center unused space rather than stretching the layout independently on each
// axis.
float UiScale();
// Convert a reference-layout X coordinate into window pixels and include the horizontal centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int UiX(int CurrentX);
// Convert a reference-layout Y coordinate into window pixels and include the vertical centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int UiY(int CurrentY);
// Scale a reference length or font size into integer pixels without adding either centering
// margin. This keeps dimensions separate from the uiX and uiY position conversions.
int UiSize(int CurrentSize);
// Measure the label with its actual font metrics and reduce its size only as needed to fit the
// supplied pixel width. Preserve proportional spacing, including widths that require very small
// text.
float FittedMenuFontSize(const char* CurrentText, float MaximumSize, float Width);

// Convert the reference Y coordinate and font size to pixels, then center the label across the
// current window. Call drawCenteredAt when those values have already been scaled.
void DrawCentered(const char* CurrentText, int CurrentY, int CurrentSize, Color CurrentColor);
// Draw a label horizontally centered using a Y coordinate and font size already expressed in
// window pixels. Avoid applying reference scaling a second time to callers' calculated positions.
void DrawCenteredAt(const char* CurrentText, int CurrentY, int CurrentSize, Color CurrentColor);
// Draw a vertically spaced option list in reference coordinates, distinguishing the selected
// entry with its marker and theme colors. This rendering helper does not change the selected
// index.
void DrawOptions(const char* const* Labels, int CurrentCount, int CurrentSelected, int StartY);
// Draw the current input string at a centered pixel-space position and append a time-based
// blinking caret. Text editing and validation occur in the caller's update path.
void DrawTextEntry(const std::string& CurrentText, int CurrentY, int CurrentSize);

namespace MenuTheme {
// Derive every interface surface from one maze palette. No graphics context or random state is
// used.
struct Colors {
    Color CurrentInk, CurrentPanel, CurrentBorder, CurrentSelected, CurrentAccent, CurrentPaper, CurrentMuted, CurrentSecondary, CurrentDanger, CurrentBadge, CurrentKeycap;
    Color CurrentOnWorld, CurrentMazeTop, CurrentMazeSage, CurrentMazeOlive, CurrentMazeStone, CurrentMazeSide, CurrentMazeShade, CurrentMazeEdge,
        CurrentMazeFloor;
    Color CurrentMapWall, CurrentMapVisited, MapUnknown, MapVisible, Marker, CurrentTrack;
};
// Derive interface, map, and maze-preview color roles from one material palette. Return a
// complete value without allocating graphics resources, changing the active theme, or consuming
// random state.
Colors ColorsFor(const MazePalette& CurrentPalette);
// Replace the active menu palette and its derived color-role values between frames. Exported
// const references remain bound to the same fields, so existing drawing helpers observe updated
// colors.
void Apply(MazeTheme Theme);
// Return the currently applied menu theme identifier without modifying it. Application compares
// this with ActiveMazeTheme before requesting a palette update.
MazeTheme Current();

// Live read-only references keep existing drawing helpers and their default arguments in sync.
extern const Color& Ink;
extern const Color& Panel;
extern const Color& Border;
extern const Color& Selected;
extern const Color& Accent;
extern const Color& Paper;
extern const Color& Muted;
extern const Color& Secondary;
extern const Color& Danger;
extern const Color& Badge;
extern const Color& Keycap;
extern const Color& OnWorld;
extern const Color& MazeTop;
extern const Color& MazeSage;
extern const Color& MazeOlive;
extern const Color& MazeStone;
extern const Color& MazeSide;
extern const Color& MazeShade;
extern const Color& MazeEdge;
extern const Color& MazeFloor;
extern const Color& MapWall;
extern const Color& MapVisited;
}

// Own native-size menu fonts and rebuild their cache when the window scale changes.
class MenuTypography {
public:
    // Release cached custom font atlases through clear while the graphics context is still valid.
    // The shared raylib default font is borrowed and is never unloaded by this owner.
    ~MenuTypography();
    // Use the compiler-generated default constructor and the member initializers declared in this
    // type. No external resources are acquired here; later operations establish the object's
    // usable state.
    MenuTypography() = default;
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    MenuTypography(const MenuTypography&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    MenuTypography& operator=(const MenuTypography&) = delete;
    // Draw left-anchored text at reference UI coordinates using optional weight and width
    // fitting. Delegate measurement and font caching to drawText so all menu labels share the
    // same rasterization rules.
    void Draw(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor = MenuTheme::Paper,
              bool Bold = false, int Width = 0) const;
    // Draw text around a reference center X using the same fitting and cached fonts as
    // left-anchored text. The Y coordinate and nominal size use the shared reference UI scale.
    void Centered(const char* CurrentText, int CenterX, int CurrentY, int CurrentSize, Color CurrentColor = MenuTheme::Paper,
                  bool Bold = false, int Width = 0) const;
    // Center the visible glyph ink inside a pixel-space badge or keycap instead of centering only
    // the font's line box. This keeps short initials and key labels visually aligned in both
    // axes.
    void CenteredIn(const char* CurrentText, Rectangle CurrentBounds, int CurrentSize, Color CurrentColor = MenuTheme::Paper,
                    bool Bold = true) const;

private:
    struct Face {
        int Pixels;
        bool Bold;
        Font CurrentFont;
    };
    // Find or load a cached font for the requested pixel size and weight, clearing obsolete faces
    // when UI scale changes. Flush pending drawing before texture upload so new atlases cannot
    // disturb queued geometry.
    Font FontFor(int Pixels, bool Bold) const;
    // Flush queued UI geometry before unloading cached custom font textures, then empty the
    // cache. Keep the shared default font alive and allow later drawing to lazily rebuild
    // required faces.
    void Clear() const;
    // Fit text to its available width, select an atlas at the final pixel size, and align the
    // baseline position to the pixel grid. The center flag changes horizontal anchoring while the
    // shared typography cache owns custom fonts.
    void DrawText(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor, bool Bold, int Width,
                  bool CurrentCenter) const;
    mutable float CachedScale = 0;
    mutable std::vector<Face> Faces;
};

// Dim the current frame and draw a centered popup with scaled dimensions, border, and fill.
// Return its pixel-space rectangle so callers can align content and controls to the actual panel.
Rectangle DrawPopup(int Width, int Height, Color CurrentBorder = MenuTheme::Accent,
                    Color CurrentFill = MenuTheme::Panel, float CurrentDim = 0.7f);
// Convert a complete reference-layout rectangle into centered window-pixel coordinates. Scale
// dimensions without position offsets and transform the origin with the shared UI margins.
Rectangle UiRect(int CurrentX, int CurrentY, int Width, int Height);
// Draw the shared themed backdrop treatment with the supplied menu variation. It supplies
// background decoration only; foreground navigation and the shared corridor journey are managed
// separately.
void DrawMenuBackdrop(int Variation);
// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void DrawMenuHeading(const char* Section, const char* Title, const char* Subtitle);
// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void DrawMenuHeading(const MenuTypography& Type, const char* Section, const char* Title,
                     const char* Subtitle = "");
// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void DrawMenuFooter(const char* Hint);
// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void DrawMenuFooter(const MenuTypography& Type, const char* Hint);
// Draw a rounded themed panel and border using bounds already expressed in window pixels. The
// caller owns layout and must not pass unscaled reference coordinates.
void DrawMenuPanel(Rectangle CurrentBounds, Color CurrentBorder = MenuTheme::Border);
// Render a selected or ordinary button inside pixel-space bounds, with a fitted label, optional
// detail line, and accent marker. Input and selection changes remain in the menu's update
// function.
void DrawMenuButton(Rectangle CurrentBounds, const char* Label, bool CurrentSelected,
                    const char* CurrentDetail = nullptr, Color CurrentAccent = MenuTheme::Accent);
// Draw themed text after scaling its reference position and font size into window pixels. This
// low-level helper leaves input handling and layout decisions to the caller.
void DrawMenuText(const char* CurrentText, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor = MenuTheme::Paper);
// Convert reference bounds to pixels and reduce the label size until it fits the available width.
// Use the same measurement and drawing scale so long labels do not overflow their controls.
void DrawMenuTextFit(const char* CurrentText, int CurrentX, int CurrentY, int Width, int CurrentSize,
                     Color CurrentColor = MenuTheme::Paper);
// Select a hovered option only when the pointer moves or clicks inside its pixel-space bounds. A
// stationary pointer leaves keyboard navigation intact instead of stealing the selection every
// frame.
bool MenuPointer(Rectangle CurrentBounds, int Index, int& CurrentSelected);
// Set the pointer cursor when hovering a pixel-space rectangle and report only a fresh left click
// inside it. Merely hovering or holding the button does not repeatedly activate the control.
bool MenuClicked(Rectangle CurrentBounds);
// Return the shared back button's reference rectangle transformed into window pixels. Menus reuse
// these bounds for both its rendering and pointer hit test.
Rectangle MenuBackBounds();
// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void DrawMenuBack();
// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void DrawMenuBack(const MenuTypography& Type);
// Return the common entry dialog's confirmation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle EntryConfirmBounds();
// Return the common entry dialog's cancellation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle EntryCancelBounds();
// Draw the common entry dialog's confirmation and cancellation controls, emphasizing confirmation
// as the default action. The caller handles editing, validation, and the resulting button
// actions.
void DrawEntryActions();
