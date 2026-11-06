#include "PauseMenu.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int ACTION_COUNT = 4;
constexpr int SENSITIVITY_OPTION = ACTION_COUNT;
const char* const LABELS[ACTION_COUNT] = {"Resume", "Restart", "Change player", "Main menu"};
constexpr PauseAction ACTIONS[ACTION_COUNT] = {PauseAction::RESUME, PauseAction::RESTART,
                                               PauseAction::CHANGE_PLAYER, PauseAction::MAIN_MENU};

// Return the scaled hit rectangle for a pause action at the supplied row index.
Rectangle actionBounds(int index) {
    return uiRect(270, 202 + index * 60, 460, 49);
}
// Return the full sensitivity-panel rectangle used to select its keyboard-adjustable option.
Rectangle sensitivityBounds() {
    return uiRect(270, 457, 460, 104);
}
// Return the slider's enlarged hit area, which also defines the horizontal sensitivity mapping.
Rectangle sliderBounds() {
    return uiRect(344, 508, 296, 34);
}
// Return the scaled hit rectangle for decreasing sensitivity by one configured step.
Rectangle minusBounds() {
    return uiRect(292, 508, 34, 34);
}
// Return the scaled hit rectangle for increasing sensitivity by one configured step.
Rectangle plusBounds() {
    return uiRect(674, 508, 34, 34);
}
// Return the scaled hit rectangle for restoring the default sensitivity.
Rectangle resetBounds() {
    return uiRect(598, 572, 132, 30);
}

// Accept both an initial key press and repeat events so held adjustment keys continue changing
// sensitivity.
bool repeatedKey(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

// Draw a compact sensitivity-adjustment button with hover feedback and a centered label.
void smallButton(Rectangle bounds, const char* label) {
    bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, .15f, 6, over ? MenuTheme::Selected : MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(bounds, .15f, 6, std::max(1.0f, uiScale()), MenuTheme::Border);
    int size = std::max(10, uiSize(22));
    DrawText(label, (int)(bounds.x + (bounds.width - MeasureText(label, size)) / 2),
             (int)(bounds.y + (bounds.height - size) / 2), size, MenuTheme::Accent);
}
}

// Bind the pause menu to the application's live settings and initially select Resume.
PauseMenu::PauseMenu(GameSettings& settings) : settings(settings), selected(0) {
}

// Return selection to Resume and cancel slider dragging without changing the saved or live
// sensitivity.
void PauseMenu::reset() {
    selected = 0;
    dragging = false;
}

// Attempt to save pending preferences and retain the outcome for the inline persistence-error
// message.
void PauseMenu::persistSensitivity() {
    saveFailed = !settings.save();
}

// Process pause navigation, slider dragging and discrete sensitivity changes, saving at interaction
// boundaries and returning any chosen pause action.
PauseAction PauseMenu::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    updateMenuSelection(selected, ACTION_COUNT + 1);
    bool actionClicked = false;
    for (int i = 0; i < ACTION_COUNT; ++i)
        actionClicked |= menuPointer(actionBounds(i), i, selected);
    menuPointer(sensitivityBounds(), SENSITIVITY_OPTION, selected);
    bool discreteChange = false;
    if (selected == SENSITIVITY_OPTION) {
        int delta = (repeatedKey(KEY_RIGHT) || repeatedKey(KEY_D)) -
                    (repeatedKey(KEY_LEFT) || repeatedKey(KEY_A));
        if (delta) {
            settings.setSensitivityPercent(settings.sensitivityPercent() +
                                           delta * GameSettings::SensitivityStep);
            discreteChange = true;
        }
        if (IsKeyPressed(KEY_HOME)) {
            settings.setSensitivityPercent(GameSettings::MinSensitivity);
            discreteChange = true;
        }
        if (IsKeyPressed(KEY_END)) {
            settings.setSensitivityPercent(GameSettings::MaxSensitivity);
            discreteChange = true;
        }
    }
    if (menuClicked(minusBounds())) {
        settings.setSensitivityPercent(settings.sensitivityPercent() -
                                       GameSettings::SensitivityStep);
        discreteChange = true;
    }
    if (menuClicked(plusBounds())) {
        settings.setSensitivityPercent(settings.sensitivityPercent() +
                                       GameSettings::SensitivityStep);
        discreteChange = true;
    }
    if (menuClicked(resetBounds()) ||
        (selected == SENSITIVITY_OPTION && IsKeyPressed(KEY_BACKSPACE))) {
        selected = SENSITIVITY_OPTION;
        settings.resetSensitivity();
        discreteChange = true;
    }

    const Rectangle slider = sliderBounds();
    bool sliderHovered = CheckCollisionPointRec(GetMousePosition(), slider);
    if (sliderHovered || dragging)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    if (sliderHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dragging = true;
        selected = SENSITIVITY_OPTION;
    }
    bool finishedDrag = false;
    if (dragging) {
        selected = SENSITIVITY_OPTION;
        float ratio = std::clamp((GetMousePosition().x - slider.x) / std::max(1.0f, slider.width),
                                 0.0f, 1.0f);
        float range = (float)(GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
        int value = GameSettings::MinSensitivity +
                    (int)std::round(ratio * range / GameSettings::SensitivityStep) *
                        GameSettings::SensitivityStep;
        settings.setSensitivityPercent(value);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            dragging = false;
            finishedDrag = true;
        }
    }
    // Persist a slider change when dragging ends, or after a keyboard/button adjustment, instead of
    // writing on every drag frame.
    if (discreteChange || finishedDrag)
        persistSensitivity();

    if (IsKeyPressed(KEY_ESCAPE)) {
        dragging = false;
        if (settings.hasUnsavedChanges())
            persistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return PauseAction::RESUME;
    }
    if ((menuConfirmed() || actionClicked) && selected < ACTION_COUNT) {
        if (settings.hasUnsavedChanges())
            persistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return ACTIONS[selected];
    }
    return PauseAction::NONE;
}

// Render the pause overlay, action list, sensitivity range and current value, reset button and any
// save-failure feedback.
void PauseMenu::draw() const {
    using namespace MenuTheme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .78f));
    drawMenuPanel(uiRect(236, 52, 528, 596));
    DrawRectangle(uiX(270), uiY(83), uiSize(5), uiSize(18), Accent);
    DrawRectangle(uiX(281), uiY(83), uiSize(5), uiSize(18), Accent);
    drawMenuText("PAUSE MENU", 302, 86, 14, Accent);
    drawMenuText("Paused", 267, 120, 38);
    for (int i = 0; i < ACTION_COUNT; ++i)
        drawMenuButton(actionBounds(i), LABELS[i], i == selected);

    bool active = selected == SENSITIVITY_OPTION;
    drawMenuPanel(sensitivityBounds(), active ? Fade(Accent, .8f) : Border);
    drawMenuText("MOUSE SENSITIVITY", 292, 475, 16, active ? Paper : Muted);
    drawMenuText(TextFormat("%.2fx", settings.sensitivityMultiplier()), 638, 472, 22, Accent);
    smallButton(minusBounds(), "-");
    smallButton(plusBounds(), "+");
    Rectangle slider = sliderBounds();
    float y = (float)uiY(523);
    float ratio = (float)(settings.sensitivityPercent() - GameSettings::MinSensitivity) /
                  (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    DrawRectangle((int)slider.x, (int)y, (int)slider.width, std::max(2, uiSize(4)), Border);
    DrawRectangle((int)slider.x, (int)y, (int)(slider.width * ratio), std::max(2, uiSize(4)),
                  Accent);
    float defaultRatio = (float)(GameSettings::DefaultSensitivity - GameSettings::MinSensitivity) /
                         (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    int notch = (int)(slider.x + slider.width * defaultRatio);
    DrawLine(notch, uiY(519), notch, uiY(532), Fade(Paper, .45f));
    DrawCircleV({slider.x + slider.width * ratio, y + uiSize(2)}, (float)std::max(4, uiSize(7)),
                Paper);
    drawMenuText("0.25x", 344, 543, 10, Muted);
    drawMenuText("3.00x", 605, 543, 10, Muted);
    drawMenuText("LEFT / RIGHT  adjust", 270, 581, 12, Muted);
    drawMenuButton(resetBounds(), "Reset",
                   CheckCollisionPointRec(GetMousePosition(), resetBounds()));
    if (saveFailed)
        drawCentered("Applied for this session. Could not save settings.", 605, 12, Danger);
    drawCentered("UP / DOWN  choose    ENTER  select    ESC  resume", 627, 14, Muted);
}
