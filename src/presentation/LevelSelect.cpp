#include "LevelSelect.h"
#include "Ui.h"
#include "Input.h"
#include "../logic/Difficulty.h"
#include <cstdlib>

// Normal presets, Custom, and a separate Zero Gravity level.
static const int CUSTOM_OPTION = DIFFICULTY_COUNT;
static const int ZERO_GRAVITY_OPTION = DIFFICULTY_COUNT + 1;
static const int OPTION_COUNT = DIFFICULTY_COUNT + 2;

LevelSelect::LevelSelect()
    : selected(0), typing(false), showError(false), chosenSize(0), chosenSeconds(0),
    chosenMode(GameMode::Normal) {}

Screen LevelSelect::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (typing) {
        // typed characters: digits only, max 2
        if (editText(text, 2, [](int ch) { return ch >= '0' && ch <= '9'; }))
            showError = false;
        if (IsKeyPressed(KEY_ENTER) || menuClicked(entryConfirmBounds())) {
            int value = text.empty() ? 0 : std::atoi(text.c_str());
            if (isValidSize(value)) {
                chosenSize = value;
                chosenSeconds = customSeconds(value);
                chosenMode = GameMode::Normal;
                typing = false;
                return Screen::PLAYING;
            }
            // invalid: stay and show the error
            showError = true;
        }
        if (IsKeyPressed(KEY_ESCAPE) || menuClicked(entryCancelBounds())) {
            typing = false;
            showError = false;
        }
        return Screen::LEVEL_SELECT;
    }

    updateMenuSelection(selected, OPTION_COUNT);
    bool clicked = false;
    for (int i = 0; i < OPTION_COUNT; ++i)
        clicked |= menuPointer(uiRect(64, 210 + i * 76, 414, 66), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds())) return Screen::MAIN_MENU;

    if (menuConfirmed() || clicked) {
        if (selected < DIFFICULTY_COUNT) {
            chosenSize = getDifficulty(selected).size;
            chosenSeconds = getDifficulty(selected).seconds;
            chosenMode = GameMode::Normal;
            return Screen::PLAYING;
        }
        if (selected == ZERO_GRAVITY_OPTION) {
            const Difficulty& level = getZeroGravityLevel();
            chosenSize = level.size;
            chosenSeconds = level.seconds;
            chosenMode = GameMode::ZeroGravity;
            return Screen::PLAYING;
        }
        // Custom: start typing
        typing = true;
        text.clear();
        showError = false;
        // drop leftover typed characters
        while (GetCharPressed() > 0) {}
    }
    return Screen::LEVEL_SELECT;
}

void LevelSelect::draw() const {
    using namespace MenuTheme;
    drawMenuBackdrop(2);
    drawMenuHeading("02 / CHOOSE YOUR ROUTE", "Into the unknown.", "A short escape or a serious detour. Pick your challenge.");
    drawMenuBack();
    for (int i = 0; i < OPTION_COUNT; i++) {
        const char* label;
        const char* detail;
        if (i < DIFFICULTY_COUNT) {
            const Difficulty& d = getDifficulty(i);
            label = d.name;
            detail = TextFormat("%d x %d cells   /   %d seconds", d.size, d.size, (int)d.seconds);
        }
        else if (i == CUSTOM_OPTION) {
            label = "Custom";
            detail = TextFormat("Your dimensions   /   %d - %d cells", MIN_SIZE, MAX_SIZE);
        }
        else {
            const Difficulty& level = getZeroGravityLevel();
            label = level.name;
            detail = "25 x 25 cells   /   180 seconds";
        }
        drawMenuButton(uiRect(64, 210 + i * 76, 414, 66), label, i == selected,
            detail, i == ZERO_GRAVITY_OPTION ? Mint : Amber);
    }
    const bool zero = selected == ZERO_GRAVITY_OPTION;
    const Color accent = zero ? Mint : Amber;
    drawMenuPanel(uiRect(510, 210, 426, 370));
    drawMenuText(zero ? "A DIFFERENT PERSPECTIVE" : "THE ROUTE BRIEF", 538, 236, 14, accent);
    // Abstract corridor diagram, distinct from the animated 3D main menu.
    for (int i = 0; i < 6; ++i) {
        int x = 574 + i * 24, y = 282 + i * 17;
        DrawLine(uiX(x), uiY(y), uiX(878 - i * 19), uiY(y), Border);
        DrawLine(uiX(x), uiY(y), uiX(x), uiY(432 - i * 8), Border);
    }
    Vector2 path[] = { {550, 419}, {650, 419}, {650, 327}, {748, 327}, {748, 391}, {882, 391} };
    for (int i = 1; i < 6; ++i)
        DrawLineEx({ (float)uiX((int)path[i - 1].x), (float)uiY((int)path[i - 1].y) },
            { (float)uiX((int)path[i].x), (float)uiY((int)path[i].y) }, 3 * uiScale(), accent);
    DrawCircle(uiX(882), uiY(391), (float)uiSize(5), Mint);
    const char* names[] = { "Find your footing.", "Trust your bearings.", "Earn your escape.", "Make it your own.", "Think beyond the floor." };
    drawMenuText(names[selected], 538, 462, 26);
    drawMenuText(zero ? "Press G to attach to walls and ceilings." : "Stay on the floor. Find the green exit.",
        538, 507, 16, Muted);
    drawMenuText(selected == CUSTOM_OPTION ? "Time limit: 5 seconds per maze cell in width."
        : "The clock starts once you enter the maze.", 538, 538, 14, Muted);
    drawMenuFooter("UP / DOWN  choose     ENTER  start     ESC  back");

    if (typing) {
        drawPopup(640, 330, Amber, Panel);
        drawMenuText("YOUR OWN LABYRINTH", 208, 214, 14, Amber);
        drawMenuText(TextFormat("Choose a size, %d - %d", MIN_SIZE, MAX_SIZE), 208, 244, 30);
        drawMenuPanel(uiRect(208, 292, 584, 62));
        drawTextEntry(text, uiY(304), uiSize(36));
        if (showError)
            drawCenteredAt(TextFormat("Enter a number from %d to %d", MIN_SIZE, MAX_SIZE),
                uiY(372), uiSize(18), Danger);
        else
            drawCentered("Square maze. Normal gravity. Your challenge.", 372, 16, Muted);
        drawEntryActions();
        drawCentered("ENTER  confirm     ESC  cancel", 478, 14, Muted);
    }
}

int LevelSelect::size() const { return chosenSize; }
float LevelSelect::seconds() const { return chosenSeconds; }
GameMode LevelSelect::mode() const { return chosenMode; }
