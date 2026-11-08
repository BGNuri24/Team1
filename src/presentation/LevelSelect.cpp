#include "LevelSelect.h"
#include "../logic/Difficulty.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cstdlib>

// Place Custom after the normal presets and Zero Gravity after Custom in the selectable option
// list.
static const int ZERO_GRAVITY_OPTION = DIFFICULTY_COUNT + 1;
static const int OPTION_COUNT = DIFFICULTY_COUNT + 2;

namespace {
Rectangle levelBounds(int index) {
    if (index < DIFFICULTY_COUNT)
        return uiRect(144 + index * 248, 202, 216, 304);
    return uiRect(index == DIFFICULTY_COUNT ? 144 : 516, 548, 340, 62);
}

void levelStroke(int x1, int y1, int x2, int y2, Color color, float width = 1) {
    DrawLineEx({(float)uiX(x1), (float)uiY(y1)}, {(float)uiX(x2), (float)uiY(y2)},
               std::max(1.0f, width * uiScale()), color);
}

// Cut corners and a punched hanging hole make the presets read as brass key tags.
void levelKeyPlate(int x, int y, Color fill, Color rim) {
    const Rectangle rect = uiRect(x, y, 216, 284);
    const float cut = (float)uiSize(22), bottom = (float)uiSize(14);
    const Vector2 edge[] = {{rect.x + cut, rect.y},
                            {rect.x + rect.width - cut, rect.y},
                            {rect.x + rect.width, rect.y + cut},
                            {rect.x + rect.width, rect.y + rect.height - bottom},
                            {rect.x + rect.width - bottom, rect.y + rect.height},
                            {rect.x + bottom, rect.y + rect.height},
                            {rect.x, rect.y + rect.height - bottom},
                            {rect.x, rect.y + cut}};
    Vector2 fan[10] = {{rect.x + rect.width / 2, rect.y + rect.height / 2}};
    for (int side = 0; side < 8; ++side)
        fan[side + 1] = edge[7 - side];
    fan[9] = edge[7];
    DrawTriangleFan(fan, 10, fill);
    for (int side = 0; side < 8; ++side)
        DrawLineEx(edge[side], edge[(side + 1) % 8], std::max(1.0f, uiScale()), rim);
    const Vector2 hole = {(float)uiX(x + 108), (float)uiY(y + 34)};
    DrawCircleV(hole, 8 * uiScale(), MenuTheme::Ink);
    DrawCircleLinesV(hole, 9 * uiScale(), rim);
}

void levelClock(int x, int y, Color color) {
    DrawCircleLinesV({(float)uiX(x), (float)uiY(y)}, 7 * uiScale(), color);
    levelStroke(x, y - 4, x, y, color);
    levelStroke(x, y, x + 3, y + 2, color);
}
} // namespace

// Initialize an idle level menu with no confirmed size or duration and Normal mode as its default.
LevelSelect::LevelSelect()
    : selected(0), typing(false), showError(false), chosenSize(0), chosenSeconds(0),
      chosenMode(GameMode::Normal) {}

// Handle preset selection or validated custom-size entry, recording the chosen size, time limit and
// mode before requesting a new run.
Screen LevelSelect::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (typing) {
        // Accept at most two decimal digits and clear the previous validation message when the
        // value changes.
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
            // Keep the entry dialog open and show its validation error when the size is outside the
            // allowed range.
            showError = true;
        }
        if (IsKeyPressed(KEY_ESCAPE) || menuClicked(entryCancelBounds())) {
            typing = false;
            showError = false;
            background.detail(false);
        }
        return Screen::LEVEL_SELECT;
    }

    updateMenuSelection(selected, OPTION_COUNT);
    const int horizontal = (int)(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) -
                           (int)(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A));
    selected = (selected + horizontal + OPTION_COUNT) % OPTION_COUNT;
    bool clicked = false;
    for (int i = 0; i < OPTION_COUNT; ++i)
        clicked |= menuPointer(levelBounds(i), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds()))
        return Screen::PLAYER_SELECT;

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
        // Open a fresh custom-size entry after the preset and Zero Gravity options have been
        // handled.
        typing = true;
        background.detail(true);
        text.clear();
        showError = false;
        // Drain queued character events so the key used to open the dialog does not populate its
        // input field.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::LEVEL_SELECT;
}

// Presets hang as room-key tags; custom sizing and gravity remain separate, compact choices.
void LevelSelect::draw() const {
    using namespace MenuTheme;
    background.draw((float)GetTime());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), uiY(154), Fade(Ink, .65f), Fade(Ink, 0));
    drawMenuBack(type);
    type.centered("Levels", 500, 77, 36, Paper, true);
    levelStroke(466, 130, 534, 130, Fade(Accent, .65f));
    levelStroke(172, 164, 828, 164, Border, 4);
    levelStroke(172, 162, 828, 162, Accent);
    for (int x : {172, 828})
        DrawCircleV({(float)uiX(x), (float)uiY(164)}, 4 * uiScale(), Accent);

    for (int index = 0; index < DIFFICULTY_COUNT; ++index) {
        const Difficulty& level = getDifficulty(index);
        const bool active = index == selected;
        const int x = 144 + index * 248, center = x + 108;
        const int y = active ? 206 : 214;
        const Color rim = active ? Accent : Border;
        DrawCircleLinesV({(float)uiX(center), (float)uiY(168)}, 7 * uiScale(), Accent);
        levelStroke(center, 175, center, y + 26, Accent, 2);
        levelKeyPlate(x + 5, y + 8, Fade(Ink, .65f), Fade(Ink, 0));
        levelKeyPlate(x, y, active ? ColorLerp(Panel, Accent, .24f) : ColorLerp(Panel, Ink, .25f),
                      rim);
        levelStroke(x + 30, y + 62, x + 186, y + 62, Fade(rim, .5f));
        type.centered(level.name, center, y + 82, 24, Paper, active, 176);
        type.centered(TextFormat("%d x %d", level.size, level.size), center, y + 126, 31,
                      active ? Accent : Paper, true, 176);
        const int seconds = (int)level.seconds;
        levelClock(center - 37, y + 197, active ? Accent : Muted);
        type.centered(TextFormat("%d:%02d", seconds / 60, seconds % 60), center + 12, y + 183, 23,
                      Muted, false, 100);
        for (int pip = 0; pip < DIFFICULTY_COUNT; ++pip)
            DrawRectangleRec(uiRect(center - 26 + pip * 20, y + 242, 12, 4),
                             pip <= index ? rim : Fade(rim, .22f));
    }

    for (int index : {DIFFICULTY_COUNT, ZERO_GRAVITY_OPTION}) {
        const bool active = index == selected;
        const bool gravity = index == ZERO_GRAVITY_OPTION;
        const int x = gravity ? 516 : 144;
        const Color accent = gravity ? Secondary : Accent;
        DrawRectangleRounded(levelBounds(index), .12f, 6, active ? Selected : Panel);
        DrawRectangleRoundedLinesEx(levelBounds(index), .12f, 6, std::max(1.0f, uiScale()),
                                    active ? accent : Border);
        if (gravity) {
            DrawCircleLinesV({(float)uiX(x + 31), (float)uiY(579)}, 14 * uiScale(), accent);
            type.centeredIn("G", uiRect(x + 17, 565, 28, 28), 19, accent);
            type.draw("Zero gravity", x + 60, 557, 21, Paper, active, 248);
            const Difficulty& level = getZeroGravityLevel();
            const int seconds = (int)level.seconds;
            type.draw(
                TextFormat("%d x %d / %d:%02d", level.size, level.size, seconds / 60, seconds % 60),
                x + 60, 585, 13, Muted, false, 248);
        } else {
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    DrawRectangleRec(uiRect(x + 20 + col * 8, 568 + row * 8, 5, 5), accent);
            type.draw("Custom", x + 60, 565, 22, Paper, active, 170);
            type.draw(TextFormat("%d-%d", MIN_SIZE, MAX_SIZE), x + 258, 571, 15, Muted, false, 64);
        }
    }

    if (typing) {
        drawPopup(640, 330, Accent, Panel, .88f);
        type.draw("Custom", 208, 226, 32, Paper, true);
        type.draw("Size", 208, 273, 13, Muted);
        type.draw(TextFormat("%d-%d", MIN_SIZE, MAX_SIZE), 720, 273, 13, Muted, false, 72);
        const std::string entry = text.empty() ? "00" : text;
        type.centered(entry.c_str(), 500, 295, 46, text.empty() ? Border : Paper, true, 400);
        levelStroke(374, 356, 626, 356, Accent);
        const int size = text.empty() ? 0 : std::atoi(text.c_str());
        if (showError)
            type.centered(TextFormat("Use %d-%d", MIN_SIZE, MAX_SIZE), 500, 373, 16, Danger);
        else if (isValidSize(size)) {
            const int seconds = (int)customSeconds(size);
            levelClock(463, 382, Muted);
            type.draw(TextFormat("%d:%02d", seconds / 60, seconds % 60), 480, 369, 18, Muted);
        }
        DrawRectangleRounded(entryConfirmBounds(), .10f, 6, Selected);
        DrawRectangleRoundedLinesEx(entryConfirmBounds(), .10f, 6, std::max(1.0f, uiScale()),
                                    Accent);
        DrawRectangleRoundedLinesEx(entryCancelBounds(), .10f, 6, std::max(1.0f, uiScale()),
                                    Border);
        type.centered("Start", 405, 415, 19, Paper, true);
        type.centered("Cancel", 595, 415, 19, Muted);
    }
}

// Return the side length recorded by the latest confirmed level choice.
int LevelSelect::size() const {
    return chosenSize;
}
// Return the countdown duration recorded by the latest confirmed level choice.
float LevelSelect::seconds() const {
    return chosenSeconds;
}
// Return the gravity mode recorded by the latest confirmed level choice.
GameMode LevelSelect::mode() const {
    return chosenMode;
}
