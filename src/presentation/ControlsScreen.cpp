#include "ControlsScreen.h"
#include "Ui.h"

static void row(int y, const char* key, const char* what) {
    int size = uiSize(32);
    DrawText(key, uiX(200), uiY(y), size, GOLD);
    DrawText(what, uiX(560), uiY(y), size, LIGHTGRAY);
}

Screen ControlsScreen::update() {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER)) return Screen::MAIN_MENU;
    return Screen::CONTROLS;
}

void ControlsScreen::draw() const {
    ClearBackground(Color{ 20, 22, 30, 255 });
    drawCentered("CONTROLS", 100, 70, GOLD);
    row(240, "Mouse", "Look around");
    row(300, "W A S D / Arrows", "Move");
    row(360, "ESC", "Back");
    row(420, "ENTER", "Select");
    row(480, "G", "Aim at wall/roof to attach");
    drawCentered("Reach the green exit before time runs out", 550, 26, WHITE);
    drawCentered("Press ENTER or ESC to go back", 620, 24, GRAY);
}