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

// Return the reference-scaled hit rectangle for a pause action at a zero-based row. Keyboard
// selection and pointer interaction use the same ordering as the rendered action list.
Rectangle actionBounds(int index) {
    return uiRect(270, 202 + index * 60, 460, 49);
}
// Return the complete reference-scaled sensitivity-panel rectangle. The panel acts as a
// selectable keyboard-adjustment row as well as containing pointer controls.
Rectangle sensitivityBounds() {
    return uiRect(270, 457, 460, 104);
}
// Return the enlarged slider hit area in window pixels. The same horizontal span maps pointer
// position to the supported sensitivity range during dragging.
Rectangle sliderBounds() {
    return uiRect(344, 508, 296, 34);
}
// Return the scaled rectangle for decreasing sensitivity by one configured step. Drawing and
// pointer activation share this geometry to keep the compact control aligned.
Rectangle minusBounds() {
    return uiRect(292, 508, 34, 34);
}
// Return the scaled rectangle for increasing sensitivity by one configured step. Drawing and
// pointer activation share this geometry to keep the compact control aligned.
Rectangle plusBounds() {
    return uiRect(674, 508, 34, 34);
}
// Return the scaled rectangle for the default-sensitivity action. The hit region matches the
// reset control rendered inside the pause settings panel.
Rectangle resetBounds() {
    return uiRect(598, 572, 132, 30);
}

// Accept an initial press or a repeat event for the supplied key. Adjustment controls use this so
// holding a key keeps changing sensitivity without requiring repeated taps.
bool repeatedKey(int key) {
    return IsKeyPressed(key) || IsKeyPressedRepeat(key);
}

struct PauseColors {
    Color body, surface, selected, border, brass, text, muted;
};

// Derive near-black pause surfaces and warm brass-and-ivory control colors from the shared hotel
// theme. Return local drawing roles without modifying the underlying maze or active palette.
PauseColors pauseColors() {
    using namespace MenuTheme;
    const Color surface = ColorLerp(Ink, Panel, .12f);
    return {ColorLerp(Ink, BLACK, .45f),      surface,
            ColorLerp(surface, Accent, .12f), ColorLerp(Ink, Accent, .23f),
            ColorLerp(Accent, Panel, .30f),   Paper,
            ColorLerp(Paper, Ink, .30f)};
}

// Draw one pause action using selected or ordinary surface colors, a border, an accent marker,
// and centered text. Bounds are already in pixels while font sizing follows shared typography.
void pauseButton(Rectangle bounds, const char* label, bool active, const PauseColors& colors,
                 const MenuTypography& type, int fontSize = 21) {
    DrawRectangleRounded(bounds, .08f, 6, active ? colors.selected : colors.surface);
    DrawRectangleRoundedLinesEx(bounds, .08f, 6, std::max(1.0f, uiScale()),
                                active ? colors.brass : colors.border);
    if (active)
        DrawRectangle((int)bounds.x, (int)(bounds.y + bounds.height * .28f), std::max(2, uiSize(2)),
                      (int)(bounds.height * .44f), colors.brass);
    type.centeredIn(label, bounds, fontSize, active ? colors.text : colors.muted);
}

// Draw a compact sensitivity-control button with current hover feedback and a centered label.
// This helper renders state only; update decides whether the control changes settings.
void smallButton(Rectangle bounds, const char* label, const PauseColors& colors,
                 const MenuTypography& type) {
    const bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRounded(bounds, .12f, 6, over ? colors.selected : colors.surface);
    DrawRectangleRoundedLinesEx(bounds, .12f, 6, std::max(1.0f, uiScale()),
                                over ? colors.brass : colors.border);
    type.centeredIn(label, bounds, 22, colors.brass);
}
} // namespace

// Borrow the application's live GameSettings object and initialize the selection to Resume. The
// menu does not own settings and applies adjustments through their validated setter.
PauseMenu::PauseMenu(GameSettings& settings) : settings(settings), selected(0) {}

// Select Resume and cancel any in-progress slider drag when the pause screen opens. Preserve the
// live sensitivity and persistence state rather than resetting the player's preference.
void PauseMenu::reset() {
    selected = 0;
    dragging = false;
}

// Ask GameSettings to save any pending preference and retain whether the attempt failed. The
// drawing path uses that outcome for inline feedback without falsely reporting success.
void PauseMenu::persistSensitivity() {
    saveFailed = !settings.save();
}

// Handle action navigation, pointer controls, slider dragging, and repeated sensitivity
// adjustments. Save at interaction boundaries and return a PauseAction for Application to
// perform.
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

// Draw the dark hotel pause overlay, action selection, and opaque sensitivity controls over the
// frozen world. Show persistence feedback from update without saving or changing gameplay during
// rendering.
void PauseMenu::draw() const {
    using namespace MenuTheme;
    const PauseColors colors = pauseColors();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .84f));
    DrawRectangleRounded(uiRect(242, 60, 528, 596), .025f, 8, Fade(BLACK, .35f));
    DrawRectangleRounded(uiRect(236, 52, 528, 596), .025f, 8, colors.body);
    DrawRectangleRoundedLinesEx(uiRect(236, 52, 528, 596), .025f, 8, std::max(1.0f, uiScale()),
                                colors.border);
    DrawRectangle(uiX(485), uiY(89), uiSize(30), std::max(1, uiSize(2)), colors.brass);
    type.centered("Paused", 500, 114, 38, colors.text, true, 460);
    DrawLine(uiX(270), uiY(174), uiX(730), uiY(174), colors.border);
    for (int i = 0; i < ACTION_COUNT; ++i)
        pauseButton(actionBounds(i), LABELS[i], i == selected, colors, type);

    const bool active = selected == SENSITIVITY_OPTION;
    DrawRectangleRounded(sensitivityBounds(), .04f, 6, colors.surface);
    DrawRectangleRoundedLinesEx(sensitivityBounds(), .04f, 6, std::max(1.0f, uiScale()),
                                active ? colors.brass : colors.border);
    type.draw("Sensitivity", 292, 471, 17, active ? colors.text : colors.muted, true, 280);
    type.centered(TextFormat("%.2fx", settings.sensitivityMultiplier()), 673, 469, 21, colors.brass,
                  true, 78);
    smallButton(minusBounds(), "-", colors, type);
    smallButton(plusBounds(), "+", colors, type);
    const Rectangle slider = sliderBounds();
    const float y = (float)uiY(523);
    const float ratio = (float)(settings.sensitivityPercent() - GameSettings::MinSensitivity) /
                        (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    DrawRectangle((int)slider.x, (int)y, (int)slider.width, std::max(2, uiSize(4)), colors.border);
    DrawRectangle((int)slider.x, (int)y, (int)(slider.width * ratio), std::max(2, uiSize(4)),
                  colors.brass);
    const float defaultRatio =
        (float)(GameSettings::DefaultSensitivity - GameSettings::MinSensitivity) /
        (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    const int notch = (int)(slider.x + slider.width * defaultRatio);
    DrawLine(notch, uiY(519), notch, uiY(532), Fade(colors.text, .35f));
    DrawCircleV({slider.x + slider.width * ratio, y + uiSize(2)}, (float)std::max(4, uiSize(7)),
                colors.brass);
    type.draw("0.25x", 344, 541, 11, colors.muted, true, 72);
    type.draw("3.00x", 605, 541, 11, colors.muted, true, 72);
    type.draw("Left / Right to adjust", 270, 578, 12, colors.muted, false, 300);
    pauseButton(resetBounds(), "Reset", CheckCollisionPointRec(GetMousePosition(), resetBounds()),
                colors, type, 14);
    if (saveFailed)
        type.centered("Settings not saved", 500, 605, 12, Danger, true, 460);
    type.centered("Esc to resume", 500, 626, 12, colors.muted, true, 460);
}
