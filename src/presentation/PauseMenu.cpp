#include "PauseMenu.h"
#include "Ui.h"
#include "Input.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr int ACTION_COUNT = 4;
    constexpr int SENSITIVITY_OPTION = ACTION_COUNT;
    const char* const LABELS[ACTION_COUNT] = { "Resume", "Restart", "Change player", "Main menu" };
    constexpr PauseAction ACTIONS[ACTION_COUNT] = {
        PauseAction::RESUME, PauseAction::RESTART, PauseAction::CHANGE_PLAYER, PauseAction::MAIN_MENU
    };

    Rectangle actionBounds(int index) { return uiRect(270, 202 + index * 60, 460, 49); }
    Rectangle sensitivityBounds() { return uiRect(270, 457, 460, 104); }
    Rectangle sliderBounds() { return uiRect(344, 508, 296, 34); }
    Rectangle minusBounds() { return uiRect(292, 508, 34, 34); }
    Rectangle plusBounds() { return uiRect(674, 508, 34, 34); }
    Rectangle resetBounds() { return uiRect(598, 572, 132, 30); }

    bool repeatedKey(int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); }

    void smallButton(Rectangle bounds, const char* label) {
        bool over = CheckCollisionPointRec(GetMousePosition(), bounds);
        DrawRectangleRounded(bounds, .15f, 6, over ? Color{ 51,58,50,255 } : Color{ 24,43,49,255 });
        DrawRectangleRoundedLinesEx(bounds, .15f, 6, std::max(1.0f, uiScale()), MenuTheme::Border);
        int size = std::max(10, uiSize(22));
        DrawText(label, (int)(bounds.x + (bounds.width - MeasureText(label, size)) / 2),
            (int)(bounds.y + (bounds.height - size) / 2), size, MenuTheme::Amber);
    }
}

PauseMenu::PauseMenu(GameSettings& settings) : settings(settings), selected(0) {}

void PauseMenu::reset() { selected = 0; dragging = false; }

void PauseMenu::persistSensitivity() {
    saveFailed = !settings.save();
}

PauseAction PauseMenu::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    updateMenuSelection(selected, ACTION_COUNT + 1);
    bool actionClicked = false;
    for (int i = 0;i < ACTION_COUNT;++i)
        actionClicked |= menuPointer(actionBounds(i), i, selected);
    menuPointer(sensitivityBounds(), SENSITIVITY_OPTION, selected);
    bool discreteChange = false;
    if (selected == SENSITIVITY_OPTION) {
        int delta = (repeatedKey(KEY_RIGHT) || repeatedKey(KEY_D))
            - (repeatedKey(KEY_LEFT) || repeatedKey(KEY_A));
        if (delta) {
            settings.setSensitivityPercent(settings.sensitivityPercent() + delta * GameSettings::SensitivityStep);
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
        settings.setSensitivityPercent(settings.sensitivityPercent() - GameSettings::SensitivityStep);
        discreteChange = true;
    }
    if (menuClicked(plusBounds())) {
        settings.setSensitivityPercent(settings.sensitivityPercent() + GameSettings::SensitivityStep);
        discreteChange = true;
    }
    if (menuClicked(resetBounds()) || (selected == SENSITIVITY_OPTION && IsKeyPressed(KEY_BACKSPACE))) {
        selected = SENSITIVITY_OPTION;
        settings.resetSensitivity();
        discreteChange = true;
    }

    const Rectangle slider = sliderBounds();
    bool sliderHovered = CheckCollisionPointRec(GetMousePosition(), slider);
    if (sliderHovered || dragging) SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    if (sliderHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        dragging = true;
        selected = SENSITIVITY_OPTION;
    }
    bool finishedDrag = false;
    if (dragging) {
        selected = SENSITIVITY_OPTION;
        float ratio = std::clamp((GetMousePosition().x - slider.x) / std::max(1.0f, slider.width), 0.0f, 1.0f);
        float range = (float)(GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
        int value = GameSettings::MinSensitivity + (int)std::round(ratio * range / GameSettings::SensitivityStep) * GameSettings::SensitivityStep;
        settings.setSensitivityPercent(value);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            dragging = false;
            finishedDrag = true;
        }
    }
    // A drag writes only on release; arrow keys and buttons save once per adjustment.
    if (discreteChange || finishedDrag) persistSensitivity();

    if (IsKeyPressed(KEY_ESCAPE)) {
        dragging = false;
        if (settings.hasUnsavedChanges()) persistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return PauseAction::RESUME;
    }
    if ((menuConfirmed() || actionClicked) && selected < ACTION_COUNT) {
        if (settings.hasUnsavedChanges()) persistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return ACTIONS[selected];
    }
    return PauseAction::NONE;
}

void PauseMenu::draw() const {
    using namespace MenuTheme;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .78f));
    drawMenuPanel(uiRect(236, 52, 528, 596));
    DrawRectangle(uiX(270), uiY(83), uiSize(5), uiSize(18), Amber);
    DrawRectangle(uiX(281), uiY(83), uiSize(5), uiSize(18), Amber);
    drawMenuText("TAKE A BREATH", 302, 86, 14, Amber);
    drawMenuText("Journey paused.", 267, 120, 38);
    drawMenuText("Tune your view. Find your way.", 270, 166, 16, Muted);
    for (int i = 0;i < ACTION_COUNT;++i)
        drawMenuButton(actionBounds(i), LABELS[i], i == selected);

    bool active = selected == SENSITIVITY_OPTION;
    drawMenuPanel(sensitivityBounds(), active ? Fade(Amber, .8f) : Border);
    drawMenuText("MOUSE SENSITIVITY", 292, 475, 16, active ? Paper : Muted);
    drawMenuText(TextFormat("%.2fx", settings.sensitivityMultiplier()), 638, 472, 22, Amber);
    smallButton(minusBounds(), "-");
    smallButton(plusBounds(), "+");
    Rectangle slider = sliderBounds();
    float y = (float)uiY(523);
    float ratio = (float)(settings.sensitivityPercent() - GameSettings::MinSensitivity)
        / (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    DrawRectangle((int)slider.x, (int)y, (int)slider.width, std::max(2, uiSize(4)), Border);
    DrawRectangle((int)slider.x, (int)y, (int)(slider.width * ratio), std::max(2, uiSize(4)), Amber);
    float defaultRatio = (float)(GameSettings::DefaultSensitivity - GameSettings::MinSensitivity)
        / (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    int notch = (int)(slider.x + slider.width * defaultRatio);
    DrawLine(notch, uiY(519), notch, uiY(532), Fade(Paper, .45f));
    DrawCircleV({ slider.x + slider.width * ratio,y + uiSize(2) }, (float)std::max(4, uiSize(7)), Paper);
    drawMenuText("0.25x", 344, 543, 10, Muted);
    drawMenuText("3.00x", 605, 543, 10, Muted);
    drawMenuText("LEFT / RIGHT  adjust", 270, 581, 12, Muted);
    drawMenuButton(resetBounds(), "Reset", CheckCollisionPointRec(GetMousePosition(), resetBounds()));
    if (saveFailed)
        drawCentered("Applied for this session. Could not save settings.", 605, 12, Danger);
    drawCentered("UP / DOWN  choose    ENTER  select    ESC  resume", 627, 14, Muted);
}
