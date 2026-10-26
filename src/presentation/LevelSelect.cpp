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
    if (typing) {
        // typed characters: digits only, max 2
        if (editText(text, 2, [](int ch) { return ch >= '0' && ch <= '9'; }))
            showError = false;
        if (IsKeyPressed(KEY_ENTER)) {
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
        if (IsKeyPressed(KEY_ESCAPE)) {
            typing = false;
            showError = false;
        }
        return Screen::LEVEL_SELECT;
    }

    updateMenuSelection(selected, OPTION_COUNT);
    if (IsKeyPressed(KEY_ESCAPE)) return Screen::MAIN_MENU;

    if (menuConfirmed()) {
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
    ClearBackground(Color{ 20, 22, 30, 255 });
    drawCentered("SELECT LEVEL", 80, 70, GOLD);

    for (int i = 0; i < OPTION_COUNT; i++) {
        const char* mark = (i == selected) ? "> " : "  ";
        const char* line;
        if (i < DIFFICULTY_COUNT) {
            const Difficulty& d = getDifficulty(i);
            line = TextFormat("%s%s   %dx%d   %d sec", mark, d.name, d.size, d.size, (int)d.seconds);
        }
        else if (i == CUSTOM_OPTION) {
            line = TextFormat("%sCustom   %d-%d", mark, MIN_SIZE, MAX_SIZE);
        }
        else {
            const Difficulty& level = getZeroGravityLevel();
            line = TextFormat("%s%s   %dx%d   %d sec", mark, level.name,
                level.size, level.size, (int)level.seconds);
        }
        drawCentered(line, 220 + i * 70, 40, i == selected ? GOLD : GRAY);
    }
    drawCentered(selected == ZERO_GRAVITY_OPTION
        ? "Zero Gravity: use G to attach to walls and ceilings"
        : "Normal gravity: stay on the floor and reach the exit",
        575, 22, selected == ZERO_GRAVITY_OPTION ? SKYBLUE : LIGHTGRAY);
    drawCentered("UP/DOWN choose    ENTER start    ESC back", 630, 24, LIGHTGRAY);

    if (typing) {
        int y = (int)drawPopup(560, 260).y;

        drawCenteredAt(TextFormat("Maze size (%d-%d)", MIN_SIZE, MAX_SIZE),
            y + uiSize(30), uiSize(36), WHITE);

        drawTextEntry(text, y + uiSize(100), uiSize(60));

        if (showError)
            drawCenteredAt(TextFormat("Enter a number from %d to %d", MIN_SIZE, MAX_SIZE),
                y + uiSize(190), uiSize(26), RED);
        else
            drawCenteredAt("ENTER confirm    ESC cancel", y + uiSize(200), uiSize(24), GRAY);
    }
}

int LevelSelect::size() const { return chosenSize; }
float LevelSelect::seconds() const { return chosenSeconds; }
GameMode LevelSelect::mode() const { return chosenMode; }
