#include "ControlsScreen.h"
#include "Ui.h"
#include <algorithm>

namespace {
// Draw a keyboard-key badge and center its label using the shared menu typography and scale. The
// supplied reference coordinates keep the control guide aligned with other UI elements.
void keycap(const MenuTypography& type, const char* label, int key, int x, int y, int width = 52,
            int height = 52) {
    using namespace MenuTheme;
    const bool held = key != KEY_NULL && IsKeyDown(key);
    const Rectangle cap = uiRect(x, y, width, height);
    DrawRectangleRounded(cap, 0.10f, 6, held ? Selected : Keycap);
    DrawRectangleRoundedLinesEx(cap, 0.10f, 6, std::max(1.0f, uiScale()), held ? Accent : Border);
    DrawRectangle(uiX(x + 6), uiY(y + height - 6), uiSize(width - 12), std::max(1, uiSize(2)),
                  held ? Accent : Border);
    const int size = width > 65 ? 17 : 22;
    type.centered(label, x + width / 2, y + (height - size) / 2 - 3, size, held ? Accent : Paper,
                  true, width - 12);
}
}

// Handle confirmation or back navigation from the controls page and return its requested screen.
// This page presents instructions and does not modify gameplay bindings.
Screen ControlsScreen::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || menuClicked(menuBackBounds()))
        return Screen::MAIN_MENU;
    if (IsKeyPressed(KEY_G)) {
        ceilingPreview = !ceilingPreview;
        background.detail(ceilingPreview);
    }
    return Screen::CONTROLS;
}

// Render the controls guide over its shared menu backdrop, grouping keys with their gameplay
// actions. Drawing reads the current theme and does not consume input or advance simulation.
void ControlsScreen::draw() const {
    using namespace MenuTheme;
    background.draw((float)GetTime());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), uiY(190), Ink, Fade(Ink, 0));
    drawMenuHeading(type, "HELP / INPUT", "Controls");
    drawMenuBack(type);

    drawMenuPanel(uiRect(64, 210, 414, 380));
    type.draw("01 / MOVE + JUMP", 88, 232, 14, Accent, true);
    keycap(type, "W", KEY_W, 156, 278);
    keycap(type, "A", KEY_A, 96, 338);
    keycap(type, "S", KEY_S, 156, 338);
    keycap(type, "D", KEY_D, 216, 338);
    type.draw("Movement", 296, 294, 22, Paper, true, 158);
    type.draw("W / A / S / D", 296, 329, 16, Muted, false, 158);
    type.draw("Arrow keys also move.", 96, 396, 14, Muted);
    keycap(type, "SPACE", KEY_SPACE, 96, 426, 172, 42);
    type.draw("Jump", 292, 424, 22, Paper, true);
    type.draw("Mouse wheel also jumps.", 292, 454, 12, Muted, false, 162);
    DrawLine(uiX(88), uiY(494), uiX(454), uiY(494), Border);
    keycap(type, "E", KEY_E, 96, 514, 42, 28);
    type.draw("Map", 178, 516, 18, Paper);
    type.draw("1-2s / 3s cooldown", 286, 519, 13, Muted, false, 168);
    keycap(type, "ESC", KEY_ESCAPE, 96, 552, 62, 28);
    type.draw("Pause / back", 178, 554, 18, Paper, false, 270);

    drawMenuPanel(uiRect(504, 210, 432, 180));
    type.draw("02 / LOOK AROUND", 530, 232, 14, Accent, true);
    DrawRectangleRounded(uiRect(540, 272, 58, 80), 0.7f, 14, Keycap);
    DrawRectangleRoundedLinesEx(uiRect(540, 272, 58, 80), 0.7f, 14, 2 * uiScale(), Muted);
    DrawLine(uiX(569), uiY(273), uiX(569), uiY(306), Border);
    DrawRectangleRounded(uiRect(564, 282, 10, 19), 0.5f, 8, Secondary);
    type.draw("Move your mouse", 628, 273, 23, Paper, true, 284);
    type.draw("Turn and look freely.", 628, 309, 16, Muted, false, 284);
    type.draw("Sensitivity: pause menu.", 628, 346, 14, Muted, false, 284);

    drawMenuPanel(uiRect(504, 410, 432, 180), Border);
    type.draw("03 / ZERO GRAVITY", 530, 432, 14, Secondary, true);
    const int floorY = 550, ceilingY = 480;
    DrawRectangle(uiX(534), uiY(ceilingY), uiSize(76), uiSize(4),
                  ceilingPreview ? Secondary : Border);
    DrawRectangle(uiX(534), uiY(floorY), uiSize(76), uiSize(4),
                  ceilingPreview ? Border : Secondary);
    const int playerY = ceilingPreview ? ceilingY + 12 : floorY - 16;
    DrawRectangleRounded(uiRect(557, playerY, 24, 12), 0.15f, 6, Paper);
    DrawLineEx({(float)uiX(599), (float)uiY(495)}, {(float)uiX(599), (float)uiY(536)},
               std::max(1.0f, 2 * uiScale()), Secondary);
    const int tip = ceilingPreview ? 495 : 536;
    const int base = ceilingPreview ? 503 : 528;
    DrawLineEx({(float)uiX(593), (float)uiY(base)}, {(float)uiX(599), (float)uiY(tip)},
               std::max(1.0f, 2 * uiScale()), Secondary);
    DrawLineEx({(float)uiX(605), (float)uiY(base)}, {(float)uiX(599), (float)uiY(tip)},
               std::max(1.0f, 2 * uiScale()), Secondary);
    keycap(type, "G", KEY_G, 634, 478, 42, 42);
    type.draw("Flip gravity", 694, 477, 22, Paper, true, 218);
    type.draw("Floor / ceiling", 694, 512, 16, Muted, false, 218);
    type.draw("Avoid red barriers.", 694, 547, 14, Danger, false, 218);
    type.draw("Press G to preview", 530, 566, 12, Muted);

    DrawLine(uiX(64), uiY(624), uiX(936), uiY(624), Border);
    type.centered("ENTER / ESC  back", 848, 643, 14, Muted, false, 176);
}
