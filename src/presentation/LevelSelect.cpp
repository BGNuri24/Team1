#include "LevelSelect.h"
#include "Ui.h"
#include "../logic/Difficulty.h"
#include <cstdlib>

// presets + Custom
static const int OPTION_COUNT = DIFFICULTY_COUNT + 1;

LevelSelect::LevelSelect()
    : selected(0), typing(false), showError(false), chosenSize(0), chosenSeconds(0) {
}

Screen LevelSelect::update() {
    if (typing) {
        // typed characters: digits only, max 2
        int ch = GetCharPressed();
        while (ch > 0) {
            if (ch >= '0' && ch <= '9' && text.size() < 2) {
                text += (char)ch;
                showError = false;
            }
            ch = GetCharPressed();
        }
        if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
            text.pop_back();
            showError = false;
        }
        if (IsKeyPressed(KEY_ENTER)) {
            int value = text.empty() ? 0 : std::atoi(text.c_str());
            if (isValidSize(value)) {
                chosenSize = value;
                chosenSeconds = customSeconds(value);
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

    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % OPTION_COUNT;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + OPTION_COUNT - 1) % OPTION_COUNT;
    if (IsKeyPressed(KEY_ESCAPE)) return Screen::MAIN_MENU;

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (selected < DIFFICULTY_COUNT) {
            chosenSize = getDifficulty(selected).size;
            chosenSeconds = getDifficulty(selected).seconds;
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
        else {
            line = TextFormat("%sCustom   %d-%d", mark, MIN_SIZE, MAX_SIZE);
        }
        drawCentered(line, 220 + i * 70, 40, i == selected ? GOLD : GRAY);
    }
    drawCentered("UP/DOWN choose    ENTER start    ESC back", 620, 24, LIGHTGRAY);

    if (typing) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
        int w = uiSize(560), h = uiSize(260);
        int x = (GetScreenWidth() - w) / 2;
        int y = (GetScreenHeight() - h) / 2;
        DrawRectangle(x, y, w, h, Color{ 25, 28, 38, 255 });
        DrawRectangleLines(x, y, w, h, GOLD);

        drawCenteredAt(TextFormat("Maze size (%d-%d)", MIN_SIZE, MAX_SIZE),
            y + uiSize(30), uiSize(36), WHITE);

        int fs = uiSize(60);
        int tw = MeasureText(text.c_str(), fs);
        int tx = (GetScreenWidth() - tw) / 2;
        DrawText(text.c_str(), tx, y + uiSize(100), fs, GOLD);
        if (((int)(GetTime() * 2)) % 2 == 0)
            DrawText("_", tx + tw + uiSize(4), y + uiSize(100), fs, GOLD);

        if (showError)
            drawCenteredAt(TextFormat("Enter a number from %d to %d", MIN_SIZE, MAX_SIZE),
                y + uiSize(190), uiSize(26), RED);
        else
            drawCenteredAt("ENTER confirm    ESC cancel", y + uiSize(200), uiSize(24), GRAY);
    }
}

int LevelSelect::size() const { return chosenSize; }
float LevelSelect::seconds() const { return chosenSeconds; }