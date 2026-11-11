#include "PauseMenu.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int ActionCount = 4;
constexpr int SensitivityOption = ActionCount;
const char* const LABELS[ActionCount] = {"Resume", "Restart", "Change player", "Main menu"};
constexpr PauseAction ACTIONS[ActionCount] = {PauseAction::RESUME, PauseAction::RESTART,
                                               PauseAction::ChangePlayer, PauseAction::MainMenu};

// Return the reference-scaled hit rectangle for a pause action at a zero-based row. Keyboard
// selection and pointer interaction use the same ordering as the rendered action list.
Rectangle ActionBounds(int Index) {
    return UiRect(270, 202 + Index * 60, 460, 49);
}
// Return the complete reference-scaled sensitivity-panel rectangle. The panel acts as a
// selectable keyboard-adjustment row as well as containing pointer controls.
Rectangle SensitivityBounds() {
    return UiRect(270, 457, 460, 104);
}
// Return the enlarged slider hit area in window pixels. The same horizontal span maps pointer
// position to the supported sensitivity range during dragging.
Rectangle SliderBounds() {
    return UiRect(344, 508, 296, 34);
}
// Return the scaled rectangle for decreasing sensitivity by one configured step. Drawing and
// pointer activation share this geometry to keep the compact control aligned.
Rectangle MinusBounds() {
    return UiRect(292, 508, 34, 34);
}
// Return the scaled rectangle for increasing sensitivity by one configured step. Drawing and
// pointer activation share this geometry to keep the compact control aligned.
Rectangle PlusBounds() {
    return UiRect(674, 508, 34, 34);
}
// Return the scaled rectangle for the default-sensitivity action. The hit region matches the
// reset control rendered inside the pause settings panel.
Rectangle ResetBounds() {
    return UiRect(598, 572, 132, 30);
}

// Accept an initial press or a repeat event for the supplied key. Adjustment controls use this so
// holding a key keeps changing sensitivity without requiring repeated taps.
bool RepeatedKey(int Key) {
    return IsKeyPressed(Key) || IsKeyPressedRepeat(Key);
}

struct PauseColors {
    Color Body, Surface, CurrentSelected, CurrentBorder, Brass, CurrentText, CurrentMuted;
};

// Derive near-black pause surfaces and warm brass-and-ivory control colors from the shared hotel
// theme. Return local drawing roles without modifying the underlying maze or active palette.
PauseColors GetPauseColors() {
    using namespace MenuTheme;
    const Color Surface = ColorLerp(Ink, Panel, .12f);
    return {ColorLerp(Ink, BLACK, .45f),      Surface,
            ColorLerp(Surface, Accent, .12f), ColorLerp(Ink, Accent, .23f),
            ColorLerp(Accent, Panel, .30f),   Paper,
            ColorLerp(Paper, Ink, .30f)};
}

// Draw one pause action using selected or ordinary surface colors, a border, an accent marker,
// and centered text. Bounds are already in pixels while font sizing follows shared typography.
void PauseButton(Rectangle CurrentBounds, const char* Label, bool Active, const PauseColors& CurrentColors,
                 const MenuTypography& Type, int FontSize = 21) {
    DrawRectangleRounded(CurrentBounds, .08f, 6, Active ? CurrentColors.CurrentSelected : CurrentColors.Surface);
    DrawRectangleRoundedLinesEx(CurrentBounds, .08f, 6, std::max(1.0f, UiScale()),
                                Active ? CurrentColors.Brass : CurrentColors.CurrentBorder);
    if (Active)
        DrawRectangle((int)CurrentBounds.x, (int)(CurrentBounds.y + CurrentBounds.height * .28f), std::max(2, UiSize(2)),
                      (int)(CurrentBounds.height * .44f), CurrentColors.Brass);
    Type.CenteredIn(Label, CurrentBounds, FontSize, Active ? CurrentColors.CurrentText : CurrentColors.CurrentMuted);
}

// Draw a compact sensitivity-control button with current hover feedback and a centered label.
// This helper renders state only; update decides whether the control changes settings.
void SmallButton(Rectangle CurrentBounds, const char* Label, const PauseColors& CurrentColors,
                 const MenuTypography& Type) {
    const bool Over = CheckCollisionPointRec(GetMousePosition(), CurrentBounds);
    DrawRectangleRounded(CurrentBounds, .12f, 6, Over ? CurrentColors.CurrentSelected : CurrentColors.Surface);
    DrawRectangleRoundedLinesEx(CurrentBounds, .12f, 6, std::max(1.0f, UiScale()),
                                Over ? CurrentColors.Brass : CurrentColors.CurrentBorder);
    Type.CenteredIn(Label, CurrentBounds, 22, CurrentColors.Brass);
}
} // namespace

// Borrow the application's live GameSettings object and initialize the selection to Resume. The
// menu does not own settings and applies adjustments through their validated setter.
PauseMenu::PauseMenu(GameSettings& CurrentSettings) : CurrentSettings(CurrentSettings), CurrentSelected(0) {}

// Select Resume and cancel any in-progress slider drag when the pause screen opens. Preserve the
// live sensitivity and persistence state rather than resetting the player's preference.
void PauseMenu::Reset() {
    CurrentSelected = 0;
    Dragging = false;
}

// Ask GameSettings to save any pending preference and retain whether the attempt failed. The
// drawing path uses that outcome for inline feedback without falsely reporting success.
void PauseMenu::PersistSensitivity() {
    SaveFailed = !CurrentSettings.Save();
}

// Handle action navigation, pointer controls, slider dragging, and repeated sensitivity
// adjustments. Save at interaction boundaries and return a PauseAction for Application to
// perform.
PauseAction PauseMenu::Update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    UpdateMenuSelection(CurrentSelected, ActionCount + 1);
    bool ActionClicked = false;
    for (int I = 0; I < ActionCount; ++I)
        ActionClicked |= MenuPointer(ActionBounds(I), I, CurrentSelected);
    MenuPointer(SensitivityBounds(), SensitivityOption, CurrentSelected);
    bool DiscreteChange = false;
    if (CurrentSelected == SensitivityOption) {
        int Delta = (RepeatedKey(KEY_RIGHT) || RepeatedKey(KEY_D)) -
                    (RepeatedKey(KEY_LEFT) || RepeatedKey(KEY_A));
        if (Delta) {
            CurrentSettings.SetSensitivityPercent(CurrentSettings.SensitivityPercent() +
                                           Delta * GameSettings::SensitivityStep);
            DiscreteChange = true;
        }
        if (IsKeyPressed(KEY_HOME)) {
            CurrentSettings.SetSensitivityPercent(GameSettings::MinSensitivity);
            DiscreteChange = true;
        }
        if (IsKeyPressed(KEY_END)) {
            CurrentSettings.SetSensitivityPercent(GameSettings::MaxSensitivity);
            DiscreteChange = true;
        }
    }
    if (MenuClicked(MinusBounds())) {
        CurrentSettings.SetSensitivityPercent(CurrentSettings.SensitivityPercent() -
                                       GameSettings::SensitivityStep);
        DiscreteChange = true;
    }
    if (MenuClicked(PlusBounds())) {
        CurrentSettings.SetSensitivityPercent(CurrentSettings.SensitivityPercent() +
                                       GameSettings::SensitivityStep);
        DiscreteChange = true;
    }
    if (MenuClicked(ResetBounds()) ||
        (CurrentSelected == SensitivityOption && IsKeyPressed(KEY_BACKSPACE))) {
        CurrentSelected = SensitivityOption;
        CurrentSettings.ResetSensitivity();
        DiscreteChange = true;
    }

    const Rectangle Slider = SliderBounds();
    bool SliderHovered = CheckCollisionPointRec(GetMousePosition(), Slider);
    if (SliderHovered || Dragging)
        SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    if (SliderHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Dragging = true;
        CurrentSelected = SensitivityOption;
    }
    bool FinishedDrag = false;
    if (Dragging) {
        CurrentSelected = SensitivityOption;
        float Ratio = std::clamp((GetMousePosition().x - Slider.x) / std::max(1.0f, Slider.width),
                                 0.0f, 1.0f);
        float CurrentRange = (float)(GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
        int Value = GameSettings::MinSensitivity +
                    (int)std::round(Ratio * CurrentRange / GameSettings::SensitivityStep) *
                        GameSettings::SensitivityStep;
        CurrentSettings.SetSensitivityPercent(Value);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Dragging = false;
            FinishedDrag = true;
        }
    }
    // Persist a slider change when dragging ends, or after a keyboard/button adjustment, instead of
    // writing on every drag frame.
    if (DiscreteChange || FinishedDrag)
        PersistSensitivity();

    if (IsKeyPressed(KEY_ESCAPE)) {
        Dragging = false;
        if (CurrentSettings.HasUnsavedChanges())
            PersistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return PauseAction::RESUME;
    }
    if ((MenuConfirmed() || ActionClicked) && CurrentSelected < ActionCount) {
        if (CurrentSettings.HasUnsavedChanges())
            PersistSensitivity();
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        return ACTIONS[CurrentSelected];
    }
    return PauseAction::NONE;
}

// Draw the dark hotel pause overlay, action selection, and opaque sensitivity controls over the
// frozen world. Show persistence feedback from update without saving or changing gameplay during
// rendering.
void PauseMenu::Draw() const {
    using namespace MenuTheme;
    const PauseColors CurrentColors = GetPauseColors();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .84f));
    DrawRectangleRounded(UiRect(242, 60, 528, 596), .025f, 8, Fade(BLACK, .35f));
    DrawRectangleRounded(UiRect(236, 52, 528, 596), .025f, 8, CurrentColors.Body);
    DrawRectangleRoundedLinesEx(UiRect(236, 52, 528, 596), .025f, 8, std::max(1.0f, UiScale()),
                                CurrentColors.CurrentBorder);
    DrawRectangle(UiX(485), UiY(89), UiSize(30), std::max(1, UiSize(2)), CurrentColors.Brass);
    Type.Centered("Paused", 500, 114, 38, CurrentColors.CurrentText, true, 460);
    DrawLine(UiX(270), UiY(174), UiX(730), UiY(174), CurrentColors.CurrentBorder);
    for (int I = 0; I < ActionCount; ++I)
        PauseButton(ActionBounds(I), LABELS[I], I == CurrentSelected, CurrentColors, Type);

    const bool Active = CurrentSelected == SensitivityOption;
    DrawRectangleRounded(SensitivityBounds(), .04f, 6, CurrentColors.Surface);
    DrawRectangleRoundedLinesEx(SensitivityBounds(), .04f, 6, std::max(1.0f, UiScale()),
                                Active ? CurrentColors.Brass : CurrentColors.CurrentBorder);
    Type.Draw("Sensitivity", 292, 471, 17, Active ? CurrentColors.CurrentText : CurrentColors.CurrentMuted, true, 280);
    Type.Centered(TextFormat("%.2fx", CurrentSettings.SensitivityMultiplier()), 673, 469, 21, CurrentColors.Brass,
                  true, 78);
    SmallButton(MinusBounds(), "-", CurrentColors, Type);
    SmallButton(PlusBounds(), "+", CurrentColors, Type);
    const Rectangle Slider = SliderBounds();
    const float CurrentY = (float)UiY(523);
    const float Ratio = (float)(CurrentSettings.SensitivityPercent() - GameSettings::MinSensitivity) /
                        (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    DrawRectangle((int)Slider.x, (int)CurrentY, (int)Slider.width, std::max(2, UiSize(4)), CurrentColors.CurrentBorder);
    DrawRectangle((int)Slider.x, (int)CurrentY, (int)(Slider.width * Ratio), std::max(2, UiSize(4)),
                  CurrentColors.Brass);
    const float DefaultRatio =
        (float)(GameSettings::DefaultSensitivity - GameSettings::MinSensitivity) /
        (GameSettings::MaxSensitivity - GameSettings::MinSensitivity);
    const int Notch = (int)(Slider.x + Slider.width * DefaultRatio);
    DrawLine(Notch, UiY(519), Notch, UiY(532), Fade(CurrentColors.CurrentText, .35f));
    DrawCircleV({Slider.x + Slider.width * Ratio, CurrentY + UiSize(2)}, (float)std::max(4, UiSize(7)),
                CurrentColors.Brass);
    Type.Draw("0.25x", 344, 541, 11, CurrentColors.CurrentMuted, true, 72);
    Type.Draw("3.00x", 605, 541, 11, CurrentColors.CurrentMuted, true, 72);
    Type.Draw("Left / Right to adjust", 270, 578, 12, CurrentColors.CurrentMuted, false, 300);
    PauseButton(ResetBounds(), "Reset", CheckCollisionPointRec(GetMousePosition(), ResetBounds()),
                CurrentColors, Type, 14);
    if (SaveFailed)
        Type.Centered("Settings not saved", 500, 605, 12, Danger, true, 460);
    Type.Centered("Esc to resume", 500, 626, 12, CurrentColors.CurrentMuted, true, 460);
}
