#include "LevelSelect.h"
#include "../logic/Difficulty.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cstdlib>

// Place Custom after the normal presets and Zero Gravity after Custom in the selectable option
// list.
static const int ZeroGravityOption = DifficultyCount + 1;
static const int OptionCount = DifficultyCount + 2;

namespace {
// Return the scaled hit rectangle for a Normal preset card or one of the two lower options.
// Preset and special/custom rows use separate layouts but share pointer and drawing geometry.
Rectangle LevelBounds(int Index) {
    if (Index < DifficultyCount)
        return UiRect(144 + Index * 248, 202, 216, 304);
    return UiRect(Index == DifficultyCount ? 144 : 516, 548, 340, 62);
}

// Draw a line in the shared reference UI coordinate system with a scaled minimum-one-pixel width.
// Level cards use this for consistent borders and small decorative marks.
void LevelStroke(int X1, int Y1, int X2, int Y2, Color CurrentColor, float Width = 1) {
    DrawLineEx({(float)UiX(X1), (float)UiY(Y1)}, {(float)UiX(X2), (float)UiY(Y2)},
               std::max(1.0f, Width * UiScale()), CurrentColor);
}

// Draw a compact number or shortcut badge on a level card using the supplied colors and shared
// typography. Keep its size and label aligned with the common menu scale.
void LevelKeyPlate(int CurrentX, int CurrentY, Color CurrentFill, Color Rim) {
    const Rectangle Rect = UiRect(CurrentX, CurrentY, 216, 284);
    const float Cut = (float)UiSize(22), Bottom = (float)UiSize(14);
    const Vector2 Edge[] = {{Rect.x + Cut, Rect.y},
                            {Rect.x + Rect.width - Cut, Rect.y},
                            {Rect.x + Rect.width, Rect.y + Cut},
                            {Rect.x + Rect.width, Rect.y + Rect.height - Bottom},
                            {Rect.x + Rect.width - Bottom, Rect.y + Rect.height},
                            {Rect.x + Bottom, Rect.y + Rect.height},
                            {Rect.x, Rect.y + Rect.height - Bottom},
                            {Rect.x, Rect.y + Cut}};
    Vector2 Fan[10] = {{Rect.x + Rect.width / 2, Rect.y + Rect.height / 2}};
    for (int CurrentSide = 0; CurrentSide < 8; ++CurrentSide)
        Fan[CurrentSide + 1] = Edge[7 - CurrentSide];
    Fan[9] = Edge[7];
    DrawTriangleFan(Fan, 10, CurrentFill);
    for (int CurrentSide = 0; CurrentSide < 8; ++CurrentSide)
        DrawLineEx(Edge[CurrentSide], Edge[(CurrentSide + 1) % 8], std::max(1.0f, UiScale()), Rim);
    const Vector2 Hole = {(float)UiX(CurrentX + 108), (float)UiY(CurrentY + 34)};
    DrawCircleV(Hole, 8 * UiScale(), MenuTheme::Ink);
    DrawCircleLinesV(Hole, 9 * UiScale(), Rim);
}

// Draw a small clock outline and hands in reference UI coordinates. The level cards reuse it
// beside time limits without requiring an external icon texture.
void LevelClock(int CurrentX, int CurrentY, Color CurrentColor) {
    DrawCircleLinesV({(float)UiX(CurrentX), (float)UiY(CurrentY)}, 7 * UiScale(), CurrentColor);
    LevelStroke(CurrentX, CurrentY - 4, CurrentX, CurrentY, CurrentColor);
    LevelStroke(CurrentX, CurrentY, CurrentX + 3, CurrentY + 2, CurrentColor);
}
} // namespace

// Initialize the selected option and custom-level entry state before the screen is first shown.
// Chosen size, timer, and mode are exposed through accessors after update confirms a level.
LevelSelect::LevelSelect()
    : CurrentSelected(0), Typing(false), ShowError(false), ChosenSize(0), ChosenSeconds(0),
      ChosenMode(GameMode::Normal) {}

// Handle level-card navigation, Zero Gravity selection, and custom-size text entry. Validate a
// custom size before requesting PLAYING and retain an explanatory error when confirmation is
// invalid.
Screen LevelSelect::Update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (Typing) {
        // Accept only ASCII decimal digits while editing the two-character custom size. Range
        // validation remains separate when the player confirms the complete value.
        if (EditText(CurrentText, 2, [](int Ch) { return Ch >= '0' && Ch <= '9'; }))
            ShowError = false;
        if (IsKeyPressed(KEY_ENTER) || MenuClicked(EntryConfirmBounds())) {
            int Value = CurrentText.empty() ? 0 : std::atoi(CurrentText.c_str());
            if (IsValidSize(Value)) {
                ChosenSize = Value;
                ChosenSeconds = CustomSeconds(Value);
                ChosenMode = GameMode::Normal;
                Typing = false;
                return Screen::PLAYING;
            }
            // Keep the entry dialog open and show its validation error when the size is outside the
            // allowed range.
            ShowError = true;
        }
        if (IsKeyPressed(KEY_ESCAPE) || MenuClicked(EntryCancelBounds())) {
            Typing = false;
            ShowError = false;
            Background.Detail(false);
        }
        return Screen::LevelSelect;
    }

    UpdateMenuSelection(CurrentSelected, OptionCount);
    const int CurrentHorizontal = (int)(IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) -
                           (int)(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A));
    CurrentSelected = (CurrentSelected + CurrentHorizontal + OptionCount) % OptionCount;
    bool Clicked = false;
    for (int I = 0; I < OptionCount; ++I)
        Clicked |= MenuPointer(LevelBounds(I), I, CurrentSelected);
    if (IsKeyPressed(KEY_ESCAPE) || MenuClicked(MenuBackBounds()))
        return Screen::PlayerSelect;

    if (MenuConfirmed() || Clicked) {
        if (CurrentSelected < DifficultyCount) {
            ChosenSize = GetDifficulty(CurrentSelected).CurrentSize;
            ChosenSeconds = GetDifficulty(CurrentSelected).Seconds;
            ChosenMode = GameMode::Normal;
            return Screen::PLAYING;
        }
        if (CurrentSelected == ZeroGravityOption) {
            const Difficulty& CurrentLevel = GetZeroGravityLevel();
            ChosenSize = CurrentLevel.CurrentSize;
            ChosenSeconds = CurrentLevel.Seconds;
            ChosenMode = GameMode::ZeroGravity;
            return Screen::PLAYING;
        }
        // Open a fresh custom-size entry after the preset and Zero Gravity options have been
        // handled.
        Typing = true;
        Background.Detail(true);
        CurrentText.clear();
        ShowError = false;
        // Drain queued character events so the key used to open the dialog does not populate its
        // input field.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::LevelSelect;
}

// Render preset cards, the Zero Gravity option, and the custom-size dialog using shared theme and
// typography. Display the current selection and any validation feedback without changing chosen
// run parameters.
void LevelSelect::Draw() const {
    using namespace MenuTheme;
    Background.Draw((float)GetTime());
    DrawRectangleGradientV(0, 0, GetScreenWidth(), UiY(154), Fade(Ink, .65f), Fade(Ink, 0));
    DrawMenuBack(Type);
    Type.Centered("Levels", 500, 77, 36, Paper, true);
    LevelStroke(466, 130, 534, 130, Fade(Accent, .65f));
    LevelStroke(172, 164, 828, 164, Border, 4);
    LevelStroke(172, 162, 828, 162, Accent);
    for (int CurrentX : {172, 828})
        DrawCircleV({(float)UiX(CurrentX), (float)UiY(164)}, 4 * UiScale(), Accent);

    for (int Index = 0; Index < DifficultyCount; ++Index) {
        const Difficulty& CurrentLevel = GetDifficulty(Index);
        const bool Active = Index == CurrentSelected;
        const int CurrentX = 144 + Index * 248, CurrentCenter = CurrentX + 108;
        const int CurrentY = Active ? 206 : 214;
        const Color Rim = Active ? Accent : Border;
        DrawCircleLinesV({(float)UiX(CurrentCenter), (float)UiY(168)}, 7 * UiScale(), Accent);
        LevelStroke(CurrentCenter, 175, CurrentCenter, CurrentY + 26, Accent, 2);
        LevelKeyPlate(CurrentX + 5, CurrentY + 8, Fade(Ink, .65f), Fade(Ink, 0));
        LevelKeyPlate(CurrentX, CurrentY, Active ? ColorLerp(Panel, Accent, .24f) : ColorLerp(Panel, Ink, .25f),
                      Rim);
        LevelStroke(CurrentX + 30, CurrentY + 62, CurrentX + 186, CurrentY + 62, Fade(Rim, .5f));
        Type.Centered(CurrentLevel.CurrentName, CurrentCenter, CurrentY + 82, 24, Paper, Active, 176);
        Type.Centered(TextFormat("%d x %d", CurrentLevel.CurrentSize, CurrentLevel.CurrentSize), CurrentCenter, CurrentY + 126, 31,
                      Active ? Accent : Paper, true, 176);
        const int Seconds = (int)CurrentLevel.Seconds;
        LevelClock(CurrentCenter - 37, CurrentY + 197, Active ? Accent : Muted);
        Type.Centered(TextFormat("%d:%02d", Seconds / 60, Seconds % 60), CurrentCenter + 12, CurrentY + 183, 23,
                      Muted, false, 100);
        for (int Pip = 0; Pip < DifficultyCount; ++Pip)
            DrawRectangleRec(UiRect(CurrentCenter - 26 + Pip * 20, CurrentY + 242, 12, 4),
                             Pip <= Index ? Rim : Fade(Rim, .22f));
    }

    for (int Index : {DifficultyCount, ZeroGravityOption}) {
        const bool Active = Index == CurrentSelected;
        const bool CurrentGravity = Index == ZeroGravityOption;
        const int CurrentX = CurrentGravity ? 516 : 144;
        const Color CurrentAccent = CurrentGravity ? Secondary : Accent;
        DrawRectangleRounded(LevelBounds(Index), .12f, 6, Active ? Selected : Panel);
        DrawRectangleRoundedLinesEx(LevelBounds(Index), .12f, 6, std::max(1.0f, UiScale()),
                                    Active ? CurrentAccent : Border);
        if (CurrentGravity) {
            DrawCircleLinesV({(float)UiX(CurrentX + 31), (float)UiY(579)}, 14 * UiScale(), CurrentAccent);
            Type.CenteredIn("G", UiRect(CurrentX + 17, 565, 28, 28), 19, CurrentAccent);
            Type.Draw("Zero gravity", CurrentX + 60, 557, 21, Paper, Active, 248);
            const Difficulty& CurrentLevel = GetZeroGravityLevel();
            const int Seconds = (int)CurrentLevel.Seconds;
            Type.Draw(
                TextFormat("%d x %d / %d:%02d", CurrentLevel.CurrentSize, CurrentLevel.CurrentSize, Seconds / 60, Seconds % 60),
                CurrentX + 60, 585, 13, Muted, false, 248);
        } else {
            for (int Row = 0; Row < 3; ++Row)
                for (int Col = 0; Col < 3; ++Col)
                    DrawRectangleRec(UiRect(CurrentX + 20 + Col * 8, 568 + Row * 8, 5, 5), CurrentAccent);
            Type.Draw("Custom", CurrentX + 60, 565, 22, Paper, Active, 170);
            Type.Draw(TextFormat("%d-%d", MinSize, MaxSize), CurrentX + 258, 571, 15, Muted, false, 64);
        }
    }

    if (Typing) {
        DrawPopup(640, 330, Accent, Panel, .88f);
        Type.Draw("Custom", 208, 226, 32, Paper, true);
        Type.Draw("Size", 208, 273, 13, Muted);
        Type.Draw(TextFormat("%d-%d", MinSize, MaxSize), 720, 273, 13, Muted, false, 72);
        const std::string Entry = CurrentText.empty() ? "00" : CurrentText;
        Type.Centered(Entry.c_str(), 500, 295, 46, CurrentText.empty() ? Border : Paper, true, 400);
        LevelStroke(374, 356, 626, 356, Accent);
        const int CurrentSize = CurrentText.empty() ? 0 : std::atoi(CurrentText.c_str());
        if (ShowError)
            Type.Centered(TextFormat("Use %d-%d", MinSize, MaxSize), 500, 373, 16, Danger);
        else if (IsValidSize(CurrentSize)) {
            const int Seconds = (int)CustomSeconds(CurrentSize);
            LevelClock(463, 382, Muted);
            Type.Draw(TextFormat("%d:%02d", Seconds / 60, Seconds % 60), 480, 369, 18, Muted);
        }
        DrawRectangleRounded(EntryConfirmBounds(), .10f, 6, Selected);
        DrawRectangleRoundedLinesEx(EntryConfirmBounds(), .10f, 6, std::max(1.0f, UiScale()),
                                    Accent);
        DrawRectangleRoundedLinesEx(EntryCancelBounds(), .10f, 6, std::max(1.0f, UiScale()),
                                    Border);
        Type.Centered("Start", 405, 415, 19, Paper, true);
        Type.Centered("Cancel", 595, 415, 19, Muted);
    }
}

// Return the selected maze side length for Application::startGame. Read this after update
// confirms play; the accessor does not validate a new value or regenerate a maze.
int LevelSelect::Size() const {
    return ChosenSize;
}
// Return the selected run's time allowance in seconds, using the preset or validated custom-size
// formula. Application captures this value when creating a new Game.
float LevelSelect::Seconds() const {
    return ChosenSeconds;
}
// Return the mode associated with the selected level, distinguishing Zero Gravity from Normal
// presets and custom play. This read-only value determines which gameplay mechanics the new run
// enables.
GameMode LevelSelect::Mode() const {
    return ChosenMode;
}
