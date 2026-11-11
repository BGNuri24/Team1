#include "MazeGenerationScreen.h"
#include "Ui.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

static constexpr float BuildDuration = 4.5f;

namespace {
// Draw a thin line for the hotel-directory generation layout using shared UI scaling. Enforce a
// one-pixel minimum so separators remain visible at small resolutions.
void DirectoryRule(int X1, int Y1, int X2, int Y2, Color CurrentColor) {
    DrawLineEx({(float)UiX(X1), (float)UiY(Y1)}, {(float)UiX(X2), (float)UiY(Y2)},
               std::max(1.0f, UiScale()), CurrentColor);
}
} // namespace

// Initialize the preview for a fresh Game by resetting its displayed carving progress and
// generation timing. The maze is already generated; this screen animates its recorded opening
// sequence.
void MazeGenerationScreen::Begin(const Game& CurrentGame) {
    Elapsed = 0;
    ShownSteps = 0;
    const int CurrentSize = CurrentGame.Maze().Size();
    Revealed.assign(CurrentSize * CurrentSize, 0);
}

// Advance the generation preview from elapsed frame time and the run's recorded steps. Return
// whether its presentation has completed so Application can enter gameplay and capture the
// cursor.
bool MazeGenerationScreen::Update(const Game& CurrentGame) {
    const float Delta = GetFrameTime();
    if (std::isfinite(Delta) && Delta > 0)
        Elapsed = std::min(Elapsed + Delta, BuildDuration);
    const std::vector<MazeStep>& Steps = CurrentGame.Maze().GenerationSteps();
    const float Progress = Elapsed / BuildDuration;
    const int TargetSteps = (int)(Progress * Steps.size());
    while (ShownSteps < TargetSteps) {
        const MazeStep& Step = Steps[ShownSteps++];
        Revealed[Step.Row * CurrentGame.Maze().Size() + Step.Col] = 1;
    }
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    return Elapsed >= BuildDuration && ShownSteps == (int)Steps.size();
}

// Render the preparation directory and current generation progress for the supplied Game. This
// reads the chosen run and preview state without changing the maze or starting its gameplay
// timer.
void MazeGenerationScreen::Draw(const Game& CurrentGame) const {
    using namespace MenuTheme;
    const int CurrentSize = CurrentGame.Maze().Size();
    const float Progress = std::clamp(Elapsed / BuildDuration, 0.0f, 1.0f);
    const float CurrentS = UiScale();
    const Color Wood = ColorLerp(Panel, Ink, .28f);
    const Color Bronze = ColorLerp(Accent, Panel, .50f);
    const Color CurrentFace = ColorLerp(Ink, Panel, .22f);
    const Color Well = ColorLerp(Ink, Panel, .06f);
    const Color Lettering = ColorLerp(Paper, Accent, .30f);
    const Color Line = ColorLerp(Bronze, Paper, .10f);
    const float Stroke = std::max(1.0f, CurrentS);

    Background.Draw((float)GetTime());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .18f));

    const Rectangle Frame = UiRect(205, 64, 590, 578);
    DrawRectangleRec(UiRect(213, 75, 590, 578), Fade(Ink, .65f));
    DrawRectangleGradientV((int)Frame.x, (int)Frame.y, (int)Frame.width, (int)Frame.height, Wood,
                           ColorLerp(Wood, Ink, .60f));
    DrawRectangleLinesEx(Frame, Stroke, ColorLerp(Wood, Bronze, .30f));
    DrawRectangleRec(UiRect(218, 77, 564, 552), CurrentFace);
    DrawRectangleLinesEx(UiRect(214, 73, 572, 560), Stroke, Bronze);
    DrawRectangleLinesEx(UiRect(219, 78, 562, 550), Stroke, Fade(Bronze, .28f));

    // Mitered frame edges and flush brass fixings keep the finish architectural.
    DirectoryRule(205, 64, 218, 77, Fade(Bronze, .45f));
    DirectoryRule(795, 64, 782, 77, Fade(Bronze, .45f));
    DirectoryRule(205, 642, 218, 629, Fade(Bronze, .24f));
    DirectoryRule(795, 642, 782, 629, Fade(Bronze, .24f));
    for (int CurrentX : {211, 789})
        for (int CurrentY : {70, 636})
            DrawCircleV({(float)UiX(CurrentX), (float)UiY(CurrentY)}, std::max(1.0f, 1.3f * CurrentS), Bronze);

    Type.Draw("FLOOR PLAN", 244, 96, 19, Lettering, true, 300);
    Type.Centered(TextFormat("%d x %d", CurrentSize, CurrentSize), 714, 99, 15, Line, true, 84);
    DirectoryRule(244, 135, 756, 135, Fade(Bronze, .60f));

    const Rectangle Inset = UiRect(284, 153, 432, 432);
    DrawRectangleGradientV((int)Inset.x, (int)Inset.y, (int)Inset.width, (int)Inset.height, Well,
                           ColorLerp(Well, CurrentFace, .35f));
    DrawRectangleLinesEx(Inset, Stroke, Fade(Bronze, .38f));

    const float Board = (float)UiSize(400);
    const float CurrentCell = Board / CurrentSize;
    const float OriginX = (float)UiX(300), OriginY = (float)UiY(169);
    // Test whether a maze cell has been reached by the current recorded-generation preview.
    // Neighbor drawing uses this to avoid displaying passages before their animation step.
    auto Visible = [&](int Row, int Col) {
        return Row >= 0 && Row < CurrentSize && Col >= 0 && Col < CurrentSize &&
               (int)Revealed.size() == CurrentSize * CurrentSize && Revealed[Row * CurrentSize + Col] != 0;
    };

    const Color Passage = ColorLerp(Well, Bronze, .18f);
    const auto& Steps = CurrentGame.Maze().GenerationSteps();
    const int Current = ShownSteps > 0 ? ShownSteps - 1 : -1;
    for (int Row = 0; Row < CurrentSize; ++Row) {
        for (int Col = 0; Col < CurrentSize; ++Col) {
            if (!Visible(Row, Col))
                continue;
            const float CurrentX = std::round(OriginX + Col * CurrentCell);
            const float CurrentY = std::round(OriginY + Row * CurrentCell);
            const float CurrentRight = std::round(OriginX + (Col + 1) * CurrentCell);
            const float Bottom = std::round(OriginY + (Row + 1) * CurrentCell);
            const bool Active = Progress < 1 && Current >= 0 && Current < (int)Steps.size() &&
                                Steps[Current].Row == Row && Steps[Current].Col == Col;
            DrawRectangleRec({CurrentX, CurrentY, CurrentRight - CurrentX, Bottom - CurrentY},
                             Active ? ColorLerp(Passage, Bronze, .26f) : Passage);
        }
    }

    // Continuous passage edges read as an engraved plan, including on the largest grids.
    for (int Row = 0; Row < CurrentSize; ++Row) {
        for (int Col = 0; Col < CurrentSize; ++Col) {
            if (!Visible(Row, Col))
                continue;
            const float CurrentX = std::round(OriginX + Col * CurrentCell);
            const float CurrentY = std::round(OriginY + Row * CurrentCell);
            const float CurrentRight = std::round(OriginX + (Col + 1) * CurrentCell);
            const float Bottom = std::round(OriginY + (Row + 1) * CurrentCell);
            if (!Visible(Row - 1, Col))
                DrawLineEx({CurrentX, CurrentY}, {CurrentRight, CurrentY}, Stroke, Line);
            if (!Visible(Row + 1, Col))
                DrawLineEx({CurrentX, Bottom}, {CurrentRight, Bottom}, Stroke, Line);
            if (!Visible(Row, Col - 1))
                DrawLineEx({CurrentX, CurrentY}, {CurrentX, Bottom}, Stroke, Line);
            if (!Visible(Row, Col + 1))
                DrawLineEx({CurrentRight, CurrentY}, {CurrentRight, Bottom}, Stroke, Line);
        }
    }

    Type.Draw(Progress < 1 ? "Preparing" : "Ready", 244, 601, 15, Lettering, true, 170);
    if (CurrentGame.Mode() == GameMode::ZeroGravity)
        Type.Centered("Zero gravity", 500, 603, 13, Line, true, 160);
    Type.Centered(TextFormat("%d%%", (int)std::round(Progress * 100)), 718, 592, 26, Lettering,
                  true, 80);
    const Rectangle CurrentTrack = UiRect(244, 627, 512, 2);
    DrawRectangleRec(CurrentTrack, ColorLerp(CurrentFace, Bronze, .20f));
    DrawRectangleRec({CurrentTrack.x, CurrentTrack.y, CurrentTrack.width * Progress, CurrentTrack.height}, Bronze);
}
