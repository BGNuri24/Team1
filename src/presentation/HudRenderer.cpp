#include "HudRenderer.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
namespace {
struct HudTheme {
    Color CurrentPanel, CurrentBorder, CurrentText, CurrentMuted, CurrentAccent, CurrentTrack, Warning;
    Color Unknown, Walls, Visible, Visited, Marker;
};

// Derive readable HUD material roles from the loaded run's palette independently of the current
// menu theme. This pure conversion avoids retaining colors from a previous run or consuming
// random state.
HudTheme GetHudTheme(const MazePalette& CurrentPalette) {
    const MenuTheme::Colors CurrentColors = MenuTheme::ColorsFor(CurrentPalette);
    return {CurrentColors.CurrentPanel,   CurrentColors.CurrentBorder,     CurrentColors.CurrentPaper,      CurrentColors.CurrentMuted,
            CurrentColors.CurrentAccent,  CurrentColors.CurrentTrack,      CurrentColors.CurrentDanger,     CurrentColors.MapUnknown,
            CurrentColors.CurrentMapWall, CurrentColors.MapVisible, CurrentColors.CurrentMapVisited, CurrentColors.Marker};
}

// Draw the map's panel frame while leaving its canvas responsible for the translucent world
// overlay. Keeping the tint in one layer avoids unintentionally darkening the scene twice.
void HudPanel(Rectangle CurrentBounds, Rectangle Canvas, const HudTheme& CurrentColors) {
    const Rectangle Strips[] = {{CurrentBounds.x, CurrentBounds.y, CurrentBounds.width, Canvas.y - CurrentBounds.y},
                                {CurrentBounds.x, Canvas.y + Canvas.height, CurrentBounds.width,
                                 CurrentBounds.y + CurrentBounds.height - Canvas.y - Canvas.height},
                                {CurrentBounds.x, Canvas.y, Canvas.x - CurrentBounds.x, Canvas.height},
                                {Canvas.x + Canvas.width, Canvas.y,
                                 CurrentBounds.x + CurrentBounds.width - Canvas.x - Canvas.width, Canvas.height}};
    for (const Rectangle Strip : Strips) {
        BeginScissorMode((int)Strip.x, (int)Strip.y, (int)Strip.width, (int)Strip.height);
        DrawRectangleRounded(CurrentBounds, 0.10f, 8, Fade(CurrentColors.CurrentPanel, .85f));
        EndScissorMode();
    }
    DrawRectangleRoundedLinesEx(CurrentBounds, 0.10f, 8, std::max(1.0f, UiScale()), CurrentColors.CurrentBorder);
}

// Draw a compact map-key hint while the map is hidden, with a ring indicating cooldown progress.
// Read MapPeek timing without advancing it so rendering cannot recharge the ability.
void MapKey(const MapPeek& Peek, const HudTheme& CurrentColors) {
    const int Margin = UiSize(22), Radius = std::max(12, UiSize(15));
    const Vector2 CurrentCenter = {(float)(GetScreenWidth() - Margin - Radius), (float)(Margin + Radius)};
    DrawCircleLinesV(CurrentCenter, (float)Radius, Fade(CurrentColors.CurrentAccent, .20f));
    if (Peek.Ready())
        DrawCircleLinesV(CurrentCenter, (float)Radius, Fade(CurrentColors.CurrentAccent, .8f));
    else {
        const float Progress =
            std::clamp(1 - Peek.CooldownTimeLeft() / MapPeek::CooldownSeconds, 0.0f, 1.0f);
        if (Progress > 0)
            DrawRing(CurrentCenter, Radius - std::max(1.0f, UiScale()), (float)Radius, -90,
                     -90 + Progress * 360, 64, Fade(CurrentColors.CurrentAccent, .8f));
    }
    const int CurrentSize = std::max(10, UiSize(15));
    DrawText("E", (int)CurrentCenter.x - MeasureText("E", CurrentSize) / 2, (int)CurrentCenter.y - CurrentSize / 2, CurrentSize,
             Fade(CurrentColors.CurrentText, Peek.Ready() ? .8f : .35f));
}
} // namespace
// Compose the timer and optional map UI in screen space after world rendering has finished. The
// caller controls whether gameplay supplies an active peek state or an ordinary hidden-map hint.
void HudRenderer::Draw(const Game& CurrentGame, const MazePalette& CurrentPalette, const MapPeek* CurrentMapPeek) const {
    DrawHud(CurrentGame, CurrentPalette, !CurrentMapPeek || !CurrentMapPeek->Visible());
    if (CurrentMapPeek) {
        if (CurrentMapPeek->Visible())
            DrawMinimap(CurrentGame, CurrentPalette);
        else
            MapKey(*CurrentMapPeek, GetHudTheme(CurrentPalette));
    }
}
// Draw the centered translucent countdown digits over the world without an opaque panel or
// compass. Read the Game timer only; paused and finished frames must not advance it.
void HudRenderer::DrawHud(const Game& CurrentGame, const MazePalette& CurrentPalette, bool ShowReticle) const {
    using namespace MenuTheme;
    const HudTheme CurrentColors = GetHudTheme(CurrentPalette);
    const int Margin = UiSize(22);
    // Measure the label and draw it using the HUD's requested reference position, size, and
    // color. This local callback keeps repeated HUD text aligned with the same scaling rules.
    auto CurrentText = [&](const char* Label, int CurrentX, int CurrentY, int CurrentSize, Color CurrentColor) {
        DrawText(Label, CurrentX, CurrentY, std::max(1, UiSize(CurrentSize)), CurrentColor);
    };
    const int Remaining = std::max(0, (int)std::ceil(CurrentGame.TimeLeft()));
    const Color ClockColor = Remaining <= 30 ? CurrentColors.Warning : CurrentColors.CurrentText;
    const char* Clock = TextFormat("%02d:%02d", Remaining / 60, Remaining % 60);
    const int ClockSize = std::max(10, UiSize(42));
    const int ClockX = (GetScreenWidth() - MeasureText(Clock, ClockSize)) / 2;
    const int ClockY = Margin + UiSize(10);
    const int Shadow = std::max(1, UiSize(1));
    DrawText(Clock, ClockX + Shadow, ClockY + Shadow, ClockSize, Fade(CurrentColors.Unknown, .32f));
    DrawText(Clock, ClockX, ClockY, ClockSize, Fade(ClockColor, .78f));
    const bool CurrentZero = CurrentGame.Mode() == GameMode::ZeroGravity;

    const int Footer = GetScreenHeight() - UiSize(40);
    CurrentText("ESC  pause", Margin, Footer, 15, OnWorld);
    if (CurrentZero)
        CurrentText(CurrentGame.SurfaceAttached() ? "G  return to floor" : "G  switch to ceiling", Margin,
             Footer - UiSize(28), 15, ColorLerp(Secondary, OnWorld, .65f));
    if (CurrentZero)
        CurrentText("RED BARRIERS ARE LETHAL", Margin, Footer - UiSize(52), 13,
             ColorLerp(Danger, OnWorld, .65f));
    // Keep the world reticle from showing through the map and resembling a second player marker.
    if (!ShowReticle)
        return;
    // Keep a central reticle as a stable reference while the camera rolls between surfaces.
    int Cx = GetScreenWidth() / 2, CurrentCy = GetScreenHeight() / 2;
    Color Aim = Fade(OnWorld, .8f);
    DrawLine(Cx - UiSize(5), CurrentCy, Cx - UiSize(2), CurrentCy, Aim);
    DrawLine(Cx + UiSize(2), CurrentCy, Cx + UiSize(5), CurrentCy, Aim);
    DrawLine(Cx, CurrentCy - UiSize(5), Cx, CurrentCy - UiSize(2), Aim);
    DrawLine(Cx, CurrentCy + UiSize(2), Cx, CurrentCy + UiSize(5), Aim);
}

// Draw a centered enlarged peek using only the run's revealed cells, keeping the timer and window
// edges clear. Showing a larger map never discloses unexplored passages or changes exploration
// state.
void HudRenderer::DrawMinimap(const Game& CurrentGame, const MazePalette& CurrentPalette) const {
    const HudTheme CurrentColors = GetHudTheme(CurrentPalette);
    const int CurrentSize = CurrentGame.Maze().Size();
    const int Available =
        std::max(1, std::min({(int)(std::min(GetScreenWidth(), GetScreenHeight()) * .62f),
                              GetScreenWidth() - UiSize(80), GetScreenHeight() - UiSize(160)}));
    const int CurrentCell = std::max(1, Available / CurrentSize), Board = CurrentSize * CurrentCell;
    const int Padding = std::max(12, UiSize(20));
    const int Header = std::max(30, UiSize(40)), Footer = std::max(26, UiSize(34));
    const int PanelWidth = Board + Padding * 2, PanelHeight = Board + Header + Footer;
    const int PanelX = (GetScreenWidth() - PanelWidth) / 2;
    const int PanelY = (GetScreenHeight() - PanelHeight) / 2;
    const Rectangle CurrentPanel = {(float)PanelX, (float)PanelY, (float)PanelWidth, (float)PanelHeight};
    const int CurrentX = PanelX + Padding, CurrentY = PanelY + Header;
    DrawRectangleRoundedLinesEx(
        {CurrentPanel.x + UiSize(3), CurrentPanel.y + UiSize(4), CurrentPanel.width, CurrentPanel.height}, .10f, 8,
        std::max(1.0f, 2 * UiScale()), Fade(CurrentColors.Unknown, .35f));
    HudPanel(CurrentPanel, {(float)CurrentX, (float)CurrentY, (float)Board, (float)Board}, CurrentColors);
    const int LabelSize = std::max(12, UiSize(14));
    const int LabelY = PanelY + (Header - LabelSize) / 2;
    DrawText("MAP", CurrentX, LabelY, LabelSize, CurrentColors.CurrentMuted);
    const char* Dimensions = TextFormat("%d x %d", CurrentSize, CurrentSize);
    DrawText(Dimensions, CurrentX + Board - MeasureText(Dimensions, LabelSize), LabelY, LabelSize,
             CurrentColors.CurrentAccent);
    BeginScissorMode(CurrentX, CurrentY, Board, Board);
    for (int Row = 0; Row < CurrentSize; ++Row)
        for (int Col = 0; Col < CurrentSize; ++Col) {
            int Seen = CurrentGame.SeenState(Row, Col);
            char Tile = CurrentGame.Maze().Cell(Row, Col);
            Color CurrentColor = !Seen         ? CurrentColors.Unknown
                          : Tile == '#' ? CurrentColors.Walls
                          : Tile == 'E' ? CurrentColors.Marker
                          : Seen == 2   ? CurrentColors.Visited
                                        : CurrentColors.Visible;
            DrawRectangle(CurrentX + Col * CurrentCell, CurrentY + Row * CurrentCell, CurrentCell, CurrentCell,
                          Fade(CurrentColor, Seen ? .90f : .82f));
        }
    float Px = CurrentX + (CurrentGame.X() / MazeGeometry::CellSize + .5f) * CurrentCell;
    float Py = CurrentY + (CurrentGame.Z() / MazeGeometry::CellSize + .5f) * CurrentCell;
    const float Radius = std::clamp(CurrentCell * .35f, 2.0f, std::max(3.0f, 7 * UiScale()));
    DrawCircleV({Px, Py}, Radius, CurrentColors.Marker);
    DrawLineEx({Px, Py},
               {Px + std::cos(CurrentGame.Yaw()) * Radius * 3, Py + std::sin(CurrentGame.Yaw()) * Radius * 3},
               std::max(1.0f, UiScale()), CurrentColors.CurrentText);
    EndScissorMode();
    const char* Caption = "EXPLORED AREA";
    // Respect DrawText's ten-pixel minimum font size and switch to a shorter caption when the
    // minimap label would overflow.
    int CaptionSize = std::max(10, UiSize(10));
    if (MeasureText(Caption, CaptionSize) > Board)
        Caption = "MAP";
    DrawText(Caption, CurrentX, CurrentY + Board + (Footer - CaptionSize) / 2, CaptionSize, CurrentColors.CurrentMuted);
}
