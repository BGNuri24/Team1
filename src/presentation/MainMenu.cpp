#include "MainMenu.h"
#include "Ui.h"
#include "Input.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int COUNT = 3;
const char* const LABELS[COUNT] = {"Play", "Controls", "Quit"};
constexpr Screen DESTINATIONS[COUNT] = {
    Screen::PLAYER_SELECT, Screen::CONTROLS, Screen::QUIT
};
constexpr Color AMBER = {244, 190, 102, 255};
constexpr Color PAPER = {238, 233, 215, 255};
constexpr Color MUTED = {134, 158, 166, 255};

// Return the scaled hit rectangle for one main-menu option using the shared reference-layout
// coordinates.
Rectangle optionBounds(int index) {
    return {(float)uiX(64), (float)uiY(332 + index * 72),
             (float)uiSize(330), (float)uiSize(60)};
}

// Construct the main menu's small labyrinth emblem from scaled line segments.
void drawMazeMark() {
    const float x = (float)uiX(65), y = (float)uiY(70);
    const float unit = uiScale() * 6.0f;
    const float thickness = std::max(1.0f, uiScale() * 2.0f);
    DrawLineEx({x, y + unit * 3}, {x, y}, thickness, AMBER);
    DrawLineEx({x, y}, {x + unit * 3, y}, thickness, AMBER);
    DrawLineEx({x + unit * 3, y}, {x + unit * 3, y + unit * 3}, thickness, AMBER);
    DrawLineEx({x + unit * 3, y + unit * 3}, {x + unit, y + unit * 3}, thickness, AMBER);
    DrawLineEx({x + unit, y + unit * 3}, {x + unit, y + unit}, thickness, AMBER);
}
}

// Select the first menu option while the background member builds its independent animated maze.
MainMenu::MainMenu() : selected(0) {}

// Combine keyboard selection and intentional pointer input, returning the selected destination only
// when confirmed.
Screen MainMenu::update() {
    updateMenuSelection(selected, COUNT);
    const Vector2 mouse = GetMousePosition();
    const Vector2 delta = GetMouseDelta();
    const bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool hovered = false;
    for (int index = 0; index < COUNT; ++index) {
        if (!CheckCollisionPointRec(mouse, optionBounds(index))) continue;
        hovered = true;
        if (delta.x != 0 || delta.y != 0 || clicked) selected = index;
        if (clicked) {
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            return DESTINATIONS[index];
        }
    }
    SetMouseCursor(hovered ? MOUSE_CURSOR_POINTING_HAND : MOUSE_CURSOR_DEFAULT);
    if (menuConfirmed()) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return DESTINATIONS[selected];
    }
    return Screen::MAIN_MENU;
}

// Render the animated maze background, title and navigation buttons with a moving accent on the
// selected option.
void MainMenu::draw() const {
    const float time = (float)GetTime();
    background.draw(time);

    drawMazeMark();
    DrawText("ENTER THE LABYRINTH", uiX(99), uiY(74), uiSize(14), MUTED);
    DrawText("MAZE", uiX(65), uiY(129), uiSize(104), Fade(BLACK, 0.45f));
    DrawText("MAZE", uiX(62), uiY(125), uiSize(104), PAPER);
    DrawRectangle(uiX(65), uiY(245), uiSize(48), std::max(1, uiSize(2)), AMBER);
    DrawRectangle(uiX(120), uiY(244), uiSize(4), uiSize(4), Fade(AMBER, 0.5f));
    DrawText("THE WAY OUT IS NEVER STRAIGHT.", uiX(65), uiY(267), uiSize(16), MUTED);

    for (int index = 0; index < COUNT; ++index) {
        const Rectangle bounds = optionBounds(index);
        const bool active = index == selected;
        DrawRectangleRounded(bounds, 0.12f, 8,
                              active ? Color{40, 43, 39, 225} : Color{12, 24, 31, 155});
        DrawRectangleRoundedLinesEx(bounds, 0.12f, 8, std::max(1.0f, uiScale()),
                                     active ? Fade(AMBER, 0.65f) : Color{46, 68, 73, 120});
        if (active) {
            DrawRectangle(uiX(64), (int)bounds.y + uiSize(17),
                           std::max(2, uiSize(3)), uiSize(26), AMBER);
        }
        DrawText(TextFormat("0%d", index + 1), uiX(83), (int)bounds.y + uiSize(23),
                 uiSize(14), active ? AMBER : MUTED);
        DrawText(LABELS[index], uiX(127), (int)bounds.y + uiSize(16),
                 uiSize(28), active ? PAPER : Color{162, 178, 181, 255});
        if (active) {
            const float arrowX = (float)uiX(367) + std::sin(time * 3.0f) * uiSize(2);
            const float arrowY = bounds.y + bounds.height * 0.5f;
            DrawLineEx({arrowX - uiSize(6), arrowY - uiSize(5)}, {arrowX, arrowY},
                        std::max(1.0f, uiScale() * 1.5f), AMBER);
            DrawLineEx({arrowX, arrowY}, {arrowX - uiSize(6), arrowY + uiSize(5)},
                        std::max(1.0f, uiScale() * 1.5f), AMBER);
        }
    }

    DrawText("A way out is waiting.", uiX(65), uiY(585), uiSize(18), Color{173, 185, 181, 255});
    DrawText("UP / DOWN  navigate     ENTER  select", uiX(65), uiY(630), uiSize(14), MUTED);
}
