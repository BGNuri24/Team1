#include "PauseMenu.h"
#include "Ui.h"
#include "Input.h"

static const int COUNT = 4;
static const char* LABELS[COUNT] = {"Resume", "Restart", "Change player", "Main menu"};
static constexpr PauseAction ACTIONS[COUNT] = {
    PauseAction::RESUME, PauseAction::RESTART, PauseAction::CHANGE_PLAYER, PauseAction::MAIN_MENU
};

PauseMenu::PauseMenu() : selected(0) {}

void PauseMenu::reset() { selected = 0; }

PauseAction PauseMenu::update() {
    updateMenuSelection(selected, COUNT);
    if (IsKeyPressed(KEY_ESCAPE)) return PauseAction::RESUME;
    return menuConfirmed() ? ACTIONS[selected] : PauseAction::NONE;
}

void PauseMenu::draw() const {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.65f));
    drawCentered("PAUSED", 120, 80, GOLD);
    drawOptions(LABELS, COUNT, selected, 260);
    drawCentered("UP/DOWN choose    ENTER select    ESC resume", 640, 24, LIGHTGRAY);
}
