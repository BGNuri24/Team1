#include "PauseMenu.h"
#include "Ui.h"

static const int COUNT = 4;
static const char* LABELS[COUNT] = {"Resume", "Restart", "Change player", "Main menu"};

PauseMenu::PauseMenu() : selected(0) {}

void PauseMenu::reset() { selected = 0; }

PauseAction PauseMenu::update() {
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % COUNT;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + COUNT - 1) % COUNT;
    if (IsKeyPressed(KEY_ESCAPE)) return PauseAction::RESUME;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (selected == 0) return PauseAction::RESUME;
        if (selected == 1) return PauseAction::RESTART;
        if (selected == 2) return PauseAction::CHANGE_PLAYER;
        return PauseAction::MAIN_MENU;
    }
    return PauseAction::NONE;
}

void PauseMenu::draw() const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
    drawCentered("PAUSED", 120, 80, GOLD);
    drawOptions(LABELS, COUNT, selected, 260);
    drawCentered("UP/DOWN choose    ENTER select    ESC resume", 640, 24, LIGHTGRAY);
}