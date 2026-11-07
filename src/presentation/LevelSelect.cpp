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
    return uiRect(430, 210 + index * 76, 506, 64);
}
}

// Initialize an idle level menu with no confirmed size or duration and Normal mode as its default.
LevelSelect::LevelSelect()
    : selected(0), typing(false), showError(false), chosenSize(0), chosenSeconds(0),
      chosenMode(GameMode::Normal) {
}

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
        }
        return Screen::LEVEL_SELECT;
    }

    updateMenuSelection(selected, OPTION_COUNT);
    bool clicked = false;
    for (int i = 0; i < OPTION_COUNT; ++i)
        clicked |= menuPointer(levelBounds(i), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds()))
        return Screen::MAIN_MENU;

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
        text.clear();
        showError = false;
        // Drain queued character events so the key used to open the dialog does not populate its
        // input field.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::LEVEL_SELECT;
}

// Match profile selection with a living maze, a focused level emblem and readable option cards.
void LevelSelect::draw() const {
    using namespace MenuTheme;
    const bool zero = selected == ZERO_GRAVITY_OPTION;
    const Difficulty* preview = selected < DIFFICULTY_COUNT ? &getDifficulty(selected)
                                : zero                      ? &getZeroGravityLevel()
                                                            : nullptr;
    const Color accent = zero ? Secondary : Accent;
    const std::string mark = preview ? std::to_string(preview->size) : "?";
    background.draw((float)GetTime(), -0.16f);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), uiY(190), Ink, Fade(Ink, 0));
    drawMenuHeading(type, "02 / LEVELS", "Select level",
                    "Choose a difficulty or set a custom maze size.");
    drawMenuBack(type);

    const Vector2 emblem = {(float)uiX(222), (float)uiY(332)};
    DrawPoly(emblem, 6, 94 * uiScale(), 30, Panel);
    DrawPolyLinesEx(emblem, 6, 94 * uiScale(), 30, std::max(1.0f, uiScale()), Border);
    DrawPoly(emblem, 6, 80 * uiScale(), 30, Selected);
    DrawPolyLinesEx(emblem, 6, 80 * uiScale(), 30, std::max(1.5f, 2 * uiScale()), accent);
    type.centeredIn(
        mark.c_str(),
        {emblem.x - 80 * uiScale(), emblem.y - 80 * uiScale(), 160 * uiScale(), 160 * uiScale()},
        68, Paper);
    type.centered(preview ? preview->name : "Custom", 222, 433, 25, Paper, true, 316);
    type.centered(preview ? TextFormat("%d x %d cells / %d seconds", preview->size, preview->size,
                                       (int)preview->seconds)
                          : TextFormat("Choose %d - %d cells", MIN_SIZE, MAX_SIZE),
                  222, 473, 16, Muted, false, 316);
    type.centered(zero ? "G flips floor / ceiling" : "Normal gravity", 222, 509, 14,
                  zero ? Secondary : Muted, false, 316);
    if (zero)
        type.centered("Avoid red barriers", 222, 539, 14, Danger, false, 316);
    else if (!preview)
        type.centered("Time limit: 5 seconds per cell of width", 222, 539, 13, Muted, false, 316);

    for (int index = 0; index < OPTION_COUNT; ++index) {
        const Difficulty* level = index < DIFFICULTY_COUNT       ? &getDifficulty(index)
                                  : index == ZERO_GRAVITY_OPTION ? &getZeroGravityLevel()
                                                                 : nullptr;
        const bool active = index == selected;
        const bool gravity = index == ZERO_GRAVITY_OPTION;
        const Color rowAccent = gravity ? Secondary : Accent;
        const int y = 210 + index * 76;
        const Rectangle row = levelBounds(index);
        DrawRectangleRounded(row, 0.08f, 6, active ? Selected : Panel);
        DrawRectangleRoundedLinesEx(row, 0.08f, 6, std::max(1.0f, uiScale()),
                                    active ? rowAccent : Border);
        if (active)
            DrawRectangle(uiX(430), uiY(y + 16), std::max(2, uiSize(3)), uiSize(32), rowAccent);
        DrawRectangleRounded(uiRect(446, y + 12, 40, 40), 0.12f, 6, Badge);
        const std::string badge = gravity ? "G" : level ? std::to_string(level->size) : "+";
        type.centeredIn(badge.c_str(), uiRect(446, y + 12, 40, 40), 22, active ? rowAccent : Paper);
        type.draw(level ? level->name : "Custom", 504, y + 9, 23, active ? Paper : Muted, true,
                  334);
        type.draw(level ? TextFormat("%d x %d cells / %d seconds", level->size, level->size,
                                     (int)level->seconds)
                        : TextFormat("Custom size / %d - %d cells", MIN_SIZE, MAX_SIZE),
                  505, y + 39, 12, active ? rowAccent : Muted, false, 330);
        const Color arrow = active ? rowAccent : Border;
        DrawLineEx({(float)uiX(901), (float)uiY(y + 26)}, {(float)uiX(907), (float)uiY(y + 32)},
                   std::max(1.5f, 2 * uiScale()), arrow);
        DrawLineEx({(float)uiX(907), (float)uiY(y + 32)}, {(float)uiX(901), (float)uiY(y + 38)},
                   std::max(1.5f, 2 * uiScale()), arrow);
    }
    drawMenuFooter(type, "UP / DOWN  choose     ENTER  start     ESC  back");

    if (typing) {
        drawPopup(640, 330, Accent, Panel, 0.88f);
        type.draw("CUSTOM MAZE", 208, 210, 13, Accent, true);
        type.draw(TextFormat("Choose a size, %d - %d", MIN_SIZE, MAX_SIZE), 208, 238, 32, Paper,
                  true, 584);
        drawMenuPanel(uiRect(208, 292, 584, 62));
        const std::string entry = text + ((((int)(GetTime() * 2)) % 2 == 0) ? "|" : "");
        type.centered(entry.c_str(), 500, 301, 34, Paper, true, 544);
        type.centered(showError ? TextFormat("Enter a number from %d to %d", MIN_SIZE, MAX_SIZE)
                                : "Square maze. Normal gravity.",
                      500, 371, 17, showError ? Danger : Muted, false, 584);
        DrawRectangleRounded(entryConfirmBounds(), 0.1f, 6, Selected);
        DrawRectangleRoundedLinesEx(entryConfirmBounds(), 0.1f, 6, std::max(1.0f, uiScale()),
                                    Accent);
        DrawRectangleRounded(entryCancelBounds(), 0.1f, 6, Panel);
        DrawRectangleRoundedLinesEx(entryCancelBounds(), 0.1f, 6, std::max(1.0f, uiScale()),
                                    Border);
        type.centered("Confirm", 405, 415, 19, Paper, true);
        type.centered("Cancel", 595, 415, 19, Muted, true);
        type.centered("ENTER  confirm     ESC  cancel", 500, 476, 14, Muted);
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
