#pragma once

#include "MazePalette.h"
#include "raylib.h"
#include <string>
#include <vector>

float uiScale();
int uiX(int x);
int uiY(int y);
int uiSize(int size);
// Fit text to a pixel-space width using the same proportional spacing used by DrawTextEx.
float fittedMenuFontSize(const char* text, float maximumSize, float width);

void drawCentered(const char* text, int y, int size, Color color);
void drawCenteredAt(const char* text, int y, int size, Color color);
void drawOptions(const char* const* labels, int count, int selected, int startY);
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
Colors colorsFor(const MazePalette& palette);
void apply(MazeTheme theme);
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
    ~MenuTypography();
    MenuTypography() = default;
    MenuTypography(const MenuTypography&) = delete;
    MenuTypography& operator=(const MenuTypography&) = delete;
    void draw(const char* text, int x, int y, int size, Color color = MenuTheme::Paper,
              bool bold = false, int width = 0) const;
    void centered(const char* text, int centerX, int y, int size, Color color = MenuTheme::Paper,
                  bool bold = false, int width = 0) const;
    // Center visible glyphs horizontally and vertically inside a badge or keycap.
    void centeredIn(const char* text, Rectangle bounds, int size, Color color = MenuTheme::Paper,
                    bool bold = true) const;

private:
    struct Face {
        int pixels;
        bool bold;
        Font font;
    };
    Font fontFor(int pixels, bool bold) const;
    void clear() const;
    void drawText(const char* text, int x, int y, int size, Color color, bool bold, int width,
                  bool center) const;
    mutable float cachedScale = 0;
    mutable std::vector<Face> faces;
};

Rectangle drawPopup(int width, int height, Color border = MenuTheme::Accent,
                    Color fill = MenuTheme::Panel, float dim = 0.7f);
Rectangle uiRect(int x, int y, int width, int height);
void drawMenuBackdrop(int variation);
void drawMenuHeading(const char* section, const char* title, const char* subtitle);
void drawMenuHeading(const MenuTypography& type, const char* section, const char* title,
                     const char* subtitle = "");
void drawMenuFooter(const char* hint);
void drawMenuFooter(const MenuTypography& type, const char* hint);
void drawMenuPanel(Rectangle bounds, Color border = MenuTheme::Border);
void drawMenuButton(Rectangle bounds, const char* label, bool selected,
                    const char* detail = nullptr, Color accent = MenuTheme::Accent);
void drawMenuText(const char* text, int x, int y, int size, Color color = MenuTheme::Paper);
void drawMenuTextFit(const char* text, int x, int y, int width, int size,
                     Color color = MenuTheme::Paper);
// Update the selected option only on pointer movement or a click so a stationary mouse does not
// override keyboard navigation.
bool menuPointer(Rectangle bounds, int index, int& selected);
bool menuClicked(Rectangle bounds);
Rectangle menuBackBounds();
void drawMenuBack();
void drawMenuBack(const MenuTypography& type);
Rectangle entryConfirmBounds();
Rectangle entryCancelBounds();
void drawEntryActions();
