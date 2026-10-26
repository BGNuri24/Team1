#pragma once

#include "raylib.h"
#include <string>

float uiScale();
int uiX(int x);
int uiY(int y);
int uiSize(int size);

void drawCentered(const char* text, int y, int size, Color color);
void drawCenteredAt(const char* text, int y, int size, Color color);
void drawOptions(const char* const* labels, int count, int selected, int startY);
Rectangle drawPopup(int width, int height, Color border = GOLD,
    Color fill = { 25, 28, 38, 255 }, float dim = 0.7f);
void drawTextEntry(const std::string& text, int y, int size);

namespace MenuTheme {
    inline constexpr Color Ink = { 9, 19, 25, 255 };
    inline constexpr Color Panel = { 15, 31, 38, 245 };
    inline constexpr Color Border = { 43, 68, 73, 255 };
    inline constexpr Color Amber = { 244, 190, 102, 255 };
    inline constexpr Color Paper = { 238, 233, 215, 255 };
    inline constexpr Color Muted = { 134, 158, 166, 255 };
    inline constexpr Color Mint = { 113, 216, 182, 255 };
    inline constexpr Color Danger = { 235, 137, 119, 255 };
}

Rectangle uiRect(int x, int y, int width, int height);
void drawMenuBackdrop(int variation);
void drawMenuHeading(const char* section, const char* title, const char* subtitle);
void drawMenuFooter(const char* hint);
void drawMenuPanel(Rectangle bounds, Color border = MenuTheme::Border);
void drawMenuButton(Rectangle bounds, const char* label, bool selected,
    const char* detail = nullptr, Color accent = MenuTheme::Amber);
void drawMenuText(const char* text, int x, int y, int size,
    Color color = MenuTheme::Paper);
void drawMenuTextFit(const char* text, int x, int y, int width, int size,
    Color color = MenuTheme::Paper);
// Mouse selection only changes when the pointer moves or clicks.
bool menuPointer(Rectangle bounds, int index, int& selected);
bool menuClicked(Rectangle bounds);
Rectangle menuBackBounds();
void drawMenuBack();
Rectangle entryConfirmBounds();
Rectangle entryCancelBounds();
void drawEntryActions();
