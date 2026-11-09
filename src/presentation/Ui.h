#pragma once

#include "MazePalette.h"
#include "raylib.h"
#include <string>
#include <vector>

// Choose the uniform factor that fits the 1000-by-700 reference UI inside the current window.
// Coordinate helpers center unused space rather than stretching the layout independently on each
// axis.
float uiScale();
// Convert a reference-layout X coordinate into window pixels and include the horizontal centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int uiX(int x);
// Convert a reference-layout Y coordinate into window pixels and include the vertical centering
// margin. Use uiSize for lengths, which must not receive a position offset.
int uiY(int y);
// Scale a reference length or font size into integer pixels without adding either centering
// margin. This keeps dimensions separate from the uiX and uiY position conversions.
int uiSize(int size);
// Measure the label with its actual font metrics and reduce its size only as needed to fit the
// supplied pixel width. Preserve proportional spacing, including widths that require very small
// text.
float fittedMenuFontSize(const char* text, float maximumSize, float width);

// Convert the reference Y coordinate and font size to pixels, then center the label across the
// current window. Call drawCenteredAt when those values have already been scaled.
void drawCentered(const char* text, int y, int size, Color color);
// Draw a label horizontally centered using a Y coordinate and font size already expressed in
// window pixels. Avoid applying reference scaling a second time to callers' calculated positions.
void drawCenteredAt(const char* text, int y, int size, Color color);
// Draw a vertically spaced option list in reference coordinates, distinguishing the selected
// entry with its marker and theme colors. This rendering helper does not change the selected
// index.
void drawOptions(const char* const* labels, int count, int selected, int startY);
// Draw the current input string at a centered pixel-space position and append a time-based
// blinking caret. Text editing and validation occur in the caller's update path.
void drawTextEntry(const std::string& text, int y, int size);

namespace MenuTheme {
// Derive every interface surface from one maze palette. No graphics context or random state is
// used.
struct Colors {
    Color ink, panel, border, selected, accent, paper, muted, secondary, danger, badge, keycap;
    Color onWorld, mazeTop, mazeSage, mazeOlive, mazeStone, mazeSide, mazeShade, mazeEdge,
        mazeFloor;
    Color mapWall, mapVisited, mapUnknown, mapVisible, marker, track;
};
// Derive interface, map, and maze-preview color roles from one material palette. Return a
// complete value without allocating graphics resources, changing the active theme, or consuming
// random state.
Colors colorsFor(const MazePalette& palette);
// Replace the active menu palette and its derived color-role values between frames. Exported
// const references remain bound to the same fields, so existing drawing helpers observe updated
// colors.
void apply(MazeTheme theme);
// Return the currently applied menu theme identifier without modifying it. Application compares
// this with ActiveMazeTheme before requesting a palette update.
MazeTheme current();

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
    void draw(const char* text, int x, int y, int size, Color color = MenuTheme::Paper,
              bool bold = false, int width = 0) const;
    // Draw text around a reference center X using the same fitting and cached fonts as
    // left-anchored text. The Y coordinate and nominal size use the shared reference UI scale.
    void centered(const char* text, int centerX, int y, int size, Color color = MenuTheme::Paper,
                  bool bold = false, int width = 0) const;
    // Center the visible glyph ink inside a pixel-space badge or keycap instead of centering only
    // the font's line box. This keeps short initials and key labels visually aligned in both
    // axes.
    void centeredIn(const char* text, Rectangle bounds, int size, Color color = MenuTheme::Paper,
                    bool bold = true) const;

private:
    struct Face {
        int pixels;
        bool bold;
        Font font;
    };
    // Find or load a cached font for the requested pixel size and weight, clearing obsolete faces
    // when UI scale changes. Flush pending drawing before texture upload so new atlases cannot
    // disturb queued geometry.
    Font fontFor(int pixels, bool bold) const;
    // Flush queued UI geometry before unloading cached custom font textures, then empty the
    // cache. Keep the shared default font alive and allow later drawing to lazily rebuild
    // required faces.
    void clear() const;
    // Fit text to its available width, select an atlas at the final pixel size, and align the
    // baseline position to the pixel grid. The center flag changes horizontal anchoring while the
    // shared typography cache owns custom fonts.
    void drawText(const char* text, int x, int y, int size, Color color, bool bold, int width,
                  bool center) const;
    mutable float cachedScale = 0;
    mutable std::vector<Face> faces;
};

// Dim the current frame and draw a centered popup with scaled dimensions, border, and fill.
// Return its pixel-space rectangle so callers can align content and controls to the actual panel.
Rectangle drawPopup(int width, int height, Color border = MenuTheme::Accent,
                    Color fill = MenuTheme::Panel, float dim = 0.7f);
// Convert a complete reference-layout rectangle into centered window-pixel coordinates. Scale
// dimensions without position offsets and transform the origin with the shared UI margins.
Rectangle uiRect(int x, int y, int width, int height);
// Draw the shared themed backdrop treatment with the supplied menu variation. It supplies
// background decoration only; foreground navigation and the shared corridor journey are managed
// separately.
void drawMenuBackdrop(int variation);
// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void drawMenuHeading(const char* section, const char* title, const char* subtitle);
// Draw a page's section marker, title, and subtitle in the shared heading layout. The typography
// overload uses the caller's cached font owner while preserving the same reference positions.
void drawMenuHeading(const MenuTypography& type, const char* section, const char* title,
                     const char* subtitle = "");
// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void drawMenuFooter(const char* hint);
// Draw the shared footer divider and its navigation hint in reference coordinates. The typography
// overload uses the supplied font cache without changing screen state.
void drawMenuFooter(const MenuTypography& type, const char* hint);
// Draw a rounded themed panel and border using bounds already expressed in window pixels. The
// caller owns layout and must not pass unscaled reference coordinates.
void drawMenuPanel(Rectangle bounds, Color border = MenuTheme::Border);
// Render a selected or ordinary button inside pixel-space bounds, with a fitted label, optional
// detail line, and accent marker. Input and selection changes remain in the menu's update
// function.
void drawMenuButton(Rectangle bounds, const char* label, bool selected,
                    const char* detail = nullptr, Color accent = MenuTheme::Accent);
// Draw themed text after scaling its reference position and font size into window pixels. This
// low-level helper leaves input handling and layout decisions to the caller.
void drawMenuText(const char* text, int x, int y, int size, Color color = MenuTheme::Paper);
// Convert reference bounds to pixels and reduce the label size until it fits the available width.
// Use the same measurement and drawing scale so long labels do not overflow their controls.
void drawMenuTextFit(const char* text, int x, int y, int width, int size,
                     Color color = MenuTheme::Paper);
// Select a hovered option only when the pointer moves or clicks inside its pixel-space bounds. A
// stationary pointer leaves keyboard navigation intact instead of stealing the selection every
// frame.
bool menuPointer(Rectangle bounds, int index, int& selected);
// Set the pointer cursor when hovering a pixel-space rectangle and report only a fresh left click
// inside it. Merely hovering or holding the button does not repeatedly activate the control.
bool menuClicked(Rectangle bounds);
// Return the shared back button's reference rectangle transformed into window pixels. Menus reuse
// these bounds for both its rendering and pointer hit test.
Rectangle menuBackBounds();
// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void drawMenuBack();
// Draw the common back control using its current hover appearance. The typography-compatible
// overload delegates to the same rendering path; callers handle the navigation action separately.
void drawMenuBack(const MenuTypography& type);
// Return the common entry dialog's confirmation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle entryConfirmBounds();
// Return the common entry dialog's cancellation rectangle in window pixels. Shared drawing and
// hit testing use this same reference-scaled geometry.
Rectangle entryCancelBounds();
// Draw the common entry dialog's confirmation and cancellation controls, emphasizing confirmation
// as the default action. The caller handles editing, validation, and the resulting button
// actions.
void drawEntryActions();
