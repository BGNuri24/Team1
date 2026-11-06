#pragma once

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
// Solid dark surfaces and bright type keep menus and HUD readable over the animated world.
inline constexpr Color Ink = {11, 16, 23, 255};
inline constexpr Color Panel = {19, 27, 37, 255};
inline constexpr Color Border = {61, 79, 95, 255};
inline constexpr Color Selected = {33, 44, 57, 255};
inline constexpr Color Accent = {240, 172, 112, 255};
inline constexpr Color Paper = {244, 246, 249, 255};
inline constexpr Color Muted = {178, 192, 205, 255};
inline constexpr Color Secondary = {110, 222, 181, 255};
inline constexpr Color Danger = {255, 126, 126, 255};

// The decorative maze has crisp steel surfaces; playable maze materials remain independent.
inline constexpr Color MazeTop = {65, 94, 115, 255};
inline constexpr Color MazeSide = {33, 51, 68, 255};
inline constexpr Color MazeShade = {21, 33, 45, 255};
inline constexpr Color MazeEdge = {103, 148, 173, 255};
inline constexpr Color MazeFloor = {15, 23, 32, 255};
inline constexpr Color MapVisited = {49, 68, 83, 255};
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
