#include "ControlsScreen.h"
#include "Ui.h"
#include <algorithm>

namespace {
// Draw a keyboard-key badge and center its label using the shared menu typography and scale. The
// supplied reference coordinates keep the control guide aligned with other UI elements.
void DrawKeycap(const MenuTypography& Type, const char* Label, int Key, int CurrentX, int CurrentY, int Width = 52,
            int Height = 52) {
    using namespace MenuTheme;
    const bool CurrentHeld = Key != KEY_NULL && IsKeyDown(Key);
    const Rectangle Cap = UiRect(CurrentX, CurrentY, Width, Height);
    DrawRectangleRounded(Cap, 0.10f, 6, CurrentHeld ? Selected : Keycap);
    DrawRectangleRoundedLinesEx(Cap, 0.10f, 6, std::max(1.0f, UiScale()), CurrentHeld ? Accent : Border);
    DrawRectangle(UiX(CurrentX + 6), UiY(CurrentY + Height - 6), UiSize(Width - 12), std::max(1, UiSize(2)),
                  CurrentHeld ? Accent : Border);
    const int CurrentSize = Width > 65 ? 17 : 22;
    Type.Centered(Label, CurrentX + Width / 2, CurrentY + (Height - CurrentSize) / 2 - 3, CurrentSize, CurrentHeld ? Accent : Paper,
                  true, Width - 12);
}
}

// Handle confirmation or back navigation from the controls page and return its requested screen.
// This page presents instructions and does not modify gameplay bindings.
Screen ControlsScreen::Update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || MenuClicked(MenuBackBounds()))
        return Screen::MainMenu;
    if (IsKeyPressed(KEY_G)) {
        CeilingPreview = !CeilingPreview;
        Background.Detail(CeilingPreview);
    }
    return Screen::CONTROLS;
}

// Render the controls guide over its shared menu backdrop, grouping keys with their gameplay
// actions. Drawing reads the current theme and does not consume input or advance simulation.
void ControlsScreen::Draw() const {
    using namespace MenuTheme;
    Background.Draw((float)GetTime());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), UiY(190), Ink, Fade(Ink, 0));
    DrawMenuHeading(Type, "HELP / INPUT", "Controls");
    DrawMenuBack(Type);

    DrawMenuPanel(UiRect(64, 210, 414, 380));
    Type.Draw("01 / MOVE + JUMP", 88, 232, 14, Accent, true);
    DrawKeycap(Type, "W", KEY_W, 156, 278);
    DrawKeycap(Type, "A", KEY_A, 96, 338);
    DrawKeycap(Type, "S", KEY_S, 156, 338);
    DrawKeycap(Type, "D", KEY_D, 216, 338);
    Type.Draw("Movement", 296, 294, 22, Paper, true, 158);
    Type.Draw("W / A / S / D", 296, 329, 16, Muted, false, 158);
    Type.Draw("Arrow keys also move.", 96, 396, 14, Muted);
    DrawKeycap(Type, "SPACE", KEY_SPACE, 96, 426, 172, 42);
    Type.Draw("Jump", 292, 424, 22, Paper, true);
    Type.Draw("Mouse wheel also jumps.", 292, 454, 12, Muted, false, 162);
    DrawLine(UiX(88), UiY(494), UiX(454), UiY(494), Border);
    DrawKeycap(Type, "E", KEY_E, 96, 514, 42, 28);
    Type.Draw("Map", 178, 516, 18, Paper);
    Type.Draw("1-2s / 3s cooldown", 286, 519, 13, Muted, false, 168);
    DrawKeycap(Type, "ESC", KEY_ESCAPE, 96, 552, 62, 28);
    Type.Draw("Pause / back", 178, 554, 18, Paper, false, 270);

    DrawMenuPanel(UiRect(504, 210, 432, 180));
    Type.Draw("02 / LOOK AROUND", 530, 232, 14, Accent, true);
    DrawRectangleRounded(UiRect(540, 272, 58, 80), 0.7f, 14, Keycap);
    DrawRectangleRoundedLinesEx(UiRect(540, 272, 58, 80), 0.7f, 14, 2 * UiScale(), Muted);
    DrawLine(UiX(569), UiY(273), UiX(569), UiY(306), Border);
    DrawRectangleRounded(UiRect(564, 282, 10, 19), 0.5f, 8, Secondary);
    Type.Draw("Move your mouse", 628, 273, 23, Paper, true, 284);
    Type.Draw("Turn and look freely.", 628, 309, 16, Muted, false, 284);
    Type.Draw("Sensitivity: pause menu.", 628, 346, 14, Muted, false, 284);

    DrawMenuPanel(UiRect(504, 410, 432, 180), Border);
    Type.Draw("03 / ZERO GRAVITY", 530, 432, 14, Secondary, true);
    const int FloorY = 550, CeilingY = 480;
    DrawRectangle(UiX(534), UiY(CeilingY), UiSize(76), UiSize(4),
                  CeilingPreview ? Secondary : Border);
    DrawRectangle(UiX(534), UiY(FloorY), UiSize(76), UiSize(4),
                  CeilingPreview ? Border : Secondary);
    const int PlayerY = CeilingPreview ? CeilingY + 12 : FloorY - 16;
    DrawRectangleRounded(UiRect(557, PlayerY, 24, 12), 0.15f, 6, Paper);
    DrawLineEx({(float)UiX(599), (float)UiY(495)}, {(float)UiX(599), (float)UiY(536)},
               std::max(1.0f, 2 * UiScale()), Secondary);
    const int Tip = CeilingPreview ? 495 : 536;
    const int Base = CeilingPreview ? 503 : 528;
    DrawLineEx({(float)UiX(593), (float)UiY(Base)}, {(float)UiX(599), (float)UiY(Tip)},
               std::max(1.0f, 2 * UiScale()), Secondary);
    DrawLineEx({(float)UiX(605), (float)UiY(Base)}, {(float)UiX(599), (float)UiY(Tip)},
               std::max(1.0f, 2 * UiScale()), Secondary);
    DrawKeycap(Type, "G", KEY_G, 634, 478, 42, 42);
    Type.Draw("Flip gravity", 694, 477, 22, Paper, true, 218);
    Type.Draw("Floor / ceiling", 694, 512, 16, Muted, false, 218);
    Type.Draw("Avoid red barriers.", 694, 547, 14, Danger, false, 218);
    Type.Draw("Press G to preview", 530, 566, 12, Muted);

    DrawLine(UiX(64), UiY(624), UiX(936), UiY(624), Border);
    Type.Centered("ENTER / ESC  back", 848, 643, 14, Muted, false, 176);
}
