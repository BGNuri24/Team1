#include "PauseMenu.h"
#include "Ui.h"
#include "Input.h"

static const int COUNT = 4;
static const char* LABELS[COUNT] = { "Resume", "Restart", "Change player", "Main menu" };
static constexpr PauseAction ACTIONS[COUNT] = {
    PauseAction::RESUME, PauseAction::RESTART, PauseAction::CHANGE_PLAYER, PauseAction::MAIN_MENU
};

PauseMenu::PauseMenu() : selected(0) {}

void PauseMenu::reset() { selected = 0; }

PauseAction PauseMenu::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    updateMenuSelection(selected, COUNT);
    if (IsKeyPressed(KEY_ESCAPE)) return PauseAction::RESUME;
    bool clicked = false;
    for (int i = 0; i < COUNT; ++i)
        clicked |= menuPointer(uiRect(294, 250 + i * 69, 412, 53), i, selected);
    if (menuConfirmed() || clicked) {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return ACTIONS[selected];
    }
    return PauseAction::NONE;
}

void PauseMenu::draw() const {
    using namespace MenuTheme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, 0.78f));
    drawMenuPanel(uiRect(260, 103, 480, 490));
    DrawRectangle(uiX(294), uiY(137), uiSize(5), uiSize(18), Amber);
    DrawRectangle(uiX(305), uiY(137), uiSize(5), uiSize(18), Amber);
    drawMenuText("TAKE A BREATH", 326, 140, 14, Amber);
    drawMenuText("Journey paused.", 291, 177, 38);
    drawMenuText("Your way out can wait a moment.", 294, 222, 16, Muted);
    for (int i = 0; i < COUNT; ++i)
        drawMenuButton(uiRect(294, 250 + i * 69, 412, 53), LABELS[i], i == selected);
    drawCentered("UP / DOWN  choose    ENTER  select    ESC  resume", 554, 14, Muted);
}
