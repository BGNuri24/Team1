#include "ControlsScreen.h"
#include "Ui.h"

namespace {
    void keycap(const char* label, int x, int y, int width = 42) {
        drawMenuPanel(uiRect(x, y, width, 36));
        int size = uiSize(16);
        DrawText(label, uiX(x) + (uiSize(width) - MeasureText(label, size)) / 2,
            uiY(y + 10), size, MenuTheme::Amber);
    }
}

Screen ControlsScreen::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_ENTER) || menuClicked(menuBackBounds()))
        return Screen::MAIN_MENU;
    return Screen::CONTROLS;
}

void ControlsScreen::draw() const {
    using namespace MenuTheme;
    drawMenuBackdrop(3);
    drawMenuHeading("FIELD GUIDE / THE BASICS", "Find your bearings.", "Learn the controls. Then trust your sense of direction.");
    drawMenuBack();
    drawMenuPanel(uiRect(64, 214, 420, 204));
    drawMenuText("01 / KEEP MOVING", 88, 239, 14, Amber);
    keycap("W", 137, 280);
    keycap("A", 88, 323);
    keycap("S", 137, 323);
    keycap("D", 186, 323);
    drawMenuText("Walk the corridors", 252, 290, 20);
    drawMenuText("Arrow keys work too.", 252, 322, 16, Muted);
    drawMenuText("SPACE / WHEEL  jump. Time hops + air-strafe.", 88, 385, 15, Muted);
    drawMenuPanel(uiRect(516, 214, 420, 204));
    drawMenuText("02 / LOOK AROUND", 540, 239, 14, Amber);
    DrawRectangleRoundedLinesEx(uiRect(550, 279, 55, 77), 0.8f, 12, 2 * uiScale(), Mint);
    DrawLine(uiX(577), uiY(280), uiX(577), uiY(309), Mint);
    drawMenuText("Move your mouse", 638, 290, 20);
    drawMenuText("Turn and look freely.", 638, 322, 16, Muted);
    drawMenuText("Adjust sensitivity in the pause menu.", 540, 385, 16, Muted);
    drawMenuPanel(uiRect(64, 440, 420, 154));
    drawMenuText("03 / YOUR NEXT MOVE", 88, 463, 14, Amber);
    keycap("ESC", 88, 499, 64);
    drawMenuText("Pause / go back", 170, 508, 18);
    keycap("ENTER", 88, 545, 82);
    drawMenuText("Confirm a choice", 188, 554, 18);
    drawMenuPanel(uiRect(516, 440, 420, 154), Fade(Mint, 0.5f));
    drawMenuText("04 / ZERO GRAVITY ONLY", 540, 463, 14, Mint);
    keycap("G", 540, 499);
    drawMenuText("Attach to walls and ceilings", 602, 508, 18);
    drawMenuText("Jump from the floor. G switches surfaces.", 540, 558, 16, Muted);
    drawMenuFooter("REACH THE GREEN EXIT BEFORE TIME RUNS OUT.     ENTER / ESC  back");
}
