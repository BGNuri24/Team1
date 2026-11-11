#include "EndScreen.h"
#include "Input.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
// Return the scaled rectangle for one result-screen action using fixed reference spacing.
// Keyboard selection and pointer hit testing share the same action ordering.
Rectangle ActionBounds(int Index) {
    return UiRect(302 + Index * 210, 576, 186, 48);
}

// Format nonnegative elapsed seconds as minutes, seconds, and tenths using raylib's temporary
// TextFormat buffer. Consume the returned text promptly rather than retaining it across later
// formatting calls.
const char* FormatTime(float Value) {
    const int Tenths = (int)(std::max(0.0f, Value) * 10);
    return TextFormat("%02d:%02d.%d", Tenths / 600, Tenths / 10 % 60, Tenths % 10);
}

// Draw the main rounded result card at a reference-layout position plus the supplied pixel
// offsets. The same shape is reused for the card and its displaced shadow.
void DrawResultCard(float OffsetX, float OffsetY, Color CurrentColor) {
    Rectangle Body = UiRect(330, 224, 340, 312);
    Body.x += OffsetX;
    Body.y += OffsetY;
    DrawRectangleRounded(Body, 0.06f, 8, CurrentColor);
}
} // namespace

// Restart the result-card entrance animation and select its first action. Application calls this
// once when a run finishes so a previous result's animation and selection do not carry over.
void EndScreen::Reset() {
    AnimationTime = 0;
    CurrentSelected = 0;
}

// Advance result-screen animation and handle keyboard or pointer actions for retry and
// navigation. Return the requested Screen while the finished Game itself remains frozen.
Screen EndScreen::Update(float DeltaSeconds) {
    if (std::isfinite(DeltaSeconds) && DeltaSeconds > 0)
        AnimationTime += std::min(DeltaSeconds, 0.1f);
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    UpdateMenuSelection(CurrentSelected, 2);
    const bool CurrentLeft = IsKeyPressed(KEY_LEFT), CurrentRight = IsKeyPressed(KEY_RIGHT);
    if (CurrentLeft != CurrentRight)
        CurrentSelected = 1 - CurrentSelected;
    bool Clicked = false;
    for (int Index = 0; Index < 2; ++Index)
        Clicked |= MenuPointer(ActionBounds(Index), Index, CurrentSelected);
    if (IsKeyPressed(KEY_ESCAPE))
        return Screen::MainMenu;
    if (MenuConfirmed() || Clicked)
        return CurrentSelected == 0 ? Screen::PLAYING : Screen::MainMenu;
    return Screen::END;
}

// Render the frozen run's outcome, distinguishing victory, timeout, and obstacle contact. Show
// the player's time, best-time status, save failures, and selected navigation action without
// attempting persistence.
void EndScreen::Draw(const Game& CurrentGame, const std::string& PlayerName, float BestTime,
                     bool NewBestTime, bool BestTimeSaveFailed) const {
    using namespace MenuTheme;
    const bool Won = CurrentGame.Won();
    const bool Hazard = CurrentGame.GetFailureReason() == FailureReason::ObstacleHit;
    const Color Card = ColorLerp(Ink, BLACK, .45f);
    const Color Surface = ColorLerp(Ink, Panel, .12f);
    const Color Focus = ColorLerp(Surface, Accent, .12f);
    const Color Edge = ColorLerp(Ink, Accent, .23f);
    const Color Brass = ColorLerp(Accent, Panel, .30f);
    const Color CurrentQuiet = ColorLerp(Paper, Ink, .30f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .78f));

    const float Progress = std::clamp(AnimationTime / 0.4f, 0.0f, 1.0f);
    const float Lift = -12 * (1 - Progress) * (1 - Progress) * UiScale();
    rlPushMatrix();
    rlTranslatef(0, Lift, 0);
    DrawResultCard(5 * UiScale(), 7 * UiScale(), Fade(BLACK, .40f));
    DrawResultCard(0, 0, Card);
    DrawRectangleRoundedLinesEx(UiRect(330, 224, 340, 312), .06f, 8, std::max(1.0f, UiScale()),
                                Edge);

    if (Won) {
        Type.Centered("You are so", 500, 240, 29, Paper, true, 284);
        Type.Centered("cracked twin", 500, 272, 29, Paper, true, 284);
    } else {
        Type.Centered(Hazard ? "Barrier hit" : "Time's up", 500, 258, 34, Paper, true, 284);
    }
    Type.Centered(PlayerName.c_str(), 500, 311, 17, CurrentQuiet, true, 276);
    Type.Centered("Time", 500, 345, 15, CurrentQuiet, true);
    Type.Centered(FormatTime(CurrentGame.TimeTaken()), 500, 367, 46, Paper, true, 284);
    DrawLine(UiX(370), UiY(428), UiX(630), UiY(428), Edge);

    // A failed save must never be presented as a new record.
    if (BestTimeSaveFailed)
        Type.Centered("Time not saved", 500, 447, 18, Danger, true, 260);
    else if (Won && NewBestTime)
        Type.Centered("New best", 500, 447, 18, Brass, true, 260);
    else if (BestTime >= 0)
        Type.Centered(TextFormat("Best  %s", FormatTime(BestTime)), 500, 447, 18, CurrentQuiet, true, 260);

    const int CurrentSize = CurrentGame.Maze().Size();
    Type.Centered(CurrentGame.Mode() == GameMode::ZeroGravity
                      ? TextFormat("%d x %d / Zero gravity", CurrentSize, CurrentSize)
                      : TextFormat("%d x %d", CurrentSize, CurrentSize),
                  500, 494, 14, CurrentQuiet, true, 270);
    rlPopMatrix();

    for (int Index = 0; Index < 2; ++Index) {
        const bool Active = Index == CurrentSelected;
        const Rectangle Button = ActionBounds(Index);
        DrawRectangleRounded(Button, 0.10f, 6, Active ? Focus : Surface);
        DrawRectangleRoundedLinesEx(Button, 0.10f, 6, std::max(1.0f, UiScale()),
                                    Active ? Brass : Edge);
        if (Active)
            DrawRectangle((int)Button.x, (int)Button.y + UiSize(12), std::max(2, UiSize(2)),
                          UiSize(24), Brass);
        Type.Centered(Index == 0 ? "Again" : "Menu", 395 + Index * 210, 587, 20,
                      Active ? Paper : CurrentQuiet, true, 160);
    }
}
