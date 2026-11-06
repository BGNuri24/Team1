#include "HudRenderer.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
namespace {
// Normalize an angle in radians to the signed interval around zero so compass offsets follow the
// shortest turn.
float wrapAngle(float angle) {
    return std::atan2(std::sin(angle), std::cos(angle));
}

// Draw the translucent rounded backing and border shared by in-game information panels.
void hudPanel(Rectangle bounds) {
    DrawRectangleRounded(bounds, 0.10f, 8, MenuTheme::Panel);
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()), MenuTheme::Border);
}
}
// Keep HUD composition in one screen-space pass after world rendering has ended.
void HudRenderer::draw(const Game& game, const MazePalette& palette) const {
    drawHud(game, palette);
    drawMinimap(game, palette);
}
// Display the remaining time, mode, compass heading and applicable controls, with warning colors
// when time is short.
void HudRenderer::drawHud(const Game& game, const MazePalette& palette) const {
    using namespace MenuTheme;
    const int margin = uiSize(22);
    auto text = [&](const char* label, int x, int y, int size, Color color) {
        DrawText(label, x, y, std::max(1, uiSize(size)), color);
    };
    const int remaining = std::max(0, (int)std::ceil(game.timeLeft()));
    const bool urgent = remaining <= 30;
    const Color clockColor = urgent ? Danger : Paper;
    hudPanel({(float)margin, (float)margin, (float)uiSize(220), (float)uiSize(118)});
    DrawRectangle(margin, margin + uiSize(20), std::max(2, uiSize(3)), uiSize(28),
                  urgent ? Danger : Accent);
    text("TIME REMAINING", margin + uiSize(18), margin + uiSize(14), 12, Muted);
    text(TextFormat("%02d:%02d", remaining / 60, remaining % 60), margin + uiSize(16),
         margin + uiSize(35), 42, clockColor);
    const bool zero = game.mode() == GameMode::ZeroGravity;
    text(zero ? "ZERO GRAVITY" : palette.name, margin + uiSize(18), margin + uiSize(88), 13,
         zero ? Secondary : Accent);
    float duration = game.timeLeft() + game.timeTaken();
    float ratio = duration > 0 ? std::clamp(game.timeLeft() / duration, 0.0f, 1.0f) : 0;
    DrawRectangle(margin + uiSize(18), margin + uiSize(108), uiSize(184), std::max(1, uiSize(2)),
                  Border);
    DrawRectangle(margin + uiSize(18), margin + uiSize(108), (int)(uiSize(184) * ratio),
                  std::max(1, uiSize(2)), urgent ? Danger : Accent);

    // Project compass ticks relative to camera yaw, with east at +X and south at +Z in maze
    // coordinates.
    const int center = GetScreenWidth() / 2;
    const int compassWidth = uiSize(220);
    const int compassY = margin + uiSize(12);
    DrawLine(center - compassWidth / 2, compassY + uiSize(24), center + compassWidth / 2,
             compassY + uiSize(24), Fade(Border, .8f));
    for (int degree = 0; degree < 360; degree += 15) {
        float angle = degree * DEG2RAD;
        float offset = wrapAngle(angle - game.yaw());
        if (std::fabs(offset) > 1.0f)
            continue;
        int x = center + (int)(offset * compassWidth * .5f);
        DrawLine(x, compassY + uiSize(20), x, compassY + uiSize(24), Muted);
        if (degree % 90 == 0) {
            const char* direction = degree == 0     ? "E"
                                    : degree == 90  ? "S"
                                    : degree == 180 ? "W"
                                                    : "N";
            DrawText(direction, x - MeasureText(direction, uiSize(14)) / 2, compassY, uiSize(14),
                     Paper);
        }
    }
    DrawTriangle({(float)center, (float)(compassY + uiSize(29))},
                 {(float)(center - uiSize(3)), (float)(compassY + uiSize(35))},
                 {(float)(center + uiSize(3)), (float)(compassY + uiSize(35))}, Accent);

    const int footer = GetScreenHeight() - uiSize(40);
    text("ESC  pause", margin, footer, 15, Muted);
    if (zero)
        text(game.surfaceAttached() ? "G  return to floor" : "G  switch to ceiling", margin,
             footer - uiSize(28), 15, Secondary);
    if (zero)
        text("RED BARRIERS ARE LETHAL", margin, footer - uiSize(52), 13, Danger);
    text("FIND THE GREEN GATE",
         GetScreenWidth() - margin - MeasureText("FIND THE GREEN GATE", uiSize(14)), footer, 14,
         Muted);
    // Keep a central reticle as a stable reference while the camera rolls between surfaces.
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    Color aim = Fade(Paper, .8f);
    DrawLine(cx - uiSize(5), cy, cx - uiSize(2), cy, aim);
    DrawLine(cx + uiSize(2), cy, cx + uiSize(5), cy, aim);
    DrawLine(cx, cy - uiSize(5), cx, cy - uiSize(2), aim);
    DrawLine(cx, cy + uiSize(2), cx, cy + uiSize(5), aim);
}

// Draw only revealed maze cells and the player's marker, distinguishing visited passages without
// disclosing the solution.
void HudRenderer::drawMinimap(const Game& game, const MazePalette& palette) const {
    using namespace MenuTheme;
    int size = game.maze().size();
    int cell = std::max(1, uiSize(164) / size);
    int board = size * cell;
    int padding = uiSize(14), margin = uiSize(22);
    int panelWidth = board + padding * 2;
    int panelX = GetScreenWidth() - margin - panelWidth;
    hudPanel({(float)panelX, (float)margin, (float)panelWidth, (float)(board + uiSize(68))});
    int x = panelX + padding, y = margin + uiSize(35);
    DrawText("MINIMAP", x, margin + uiSize(13), uiSize(12), Muted);
    const char* dimensions = TextFormat("%d", size);
    DrawText(dimensions, panelX + panelWidth - padding - MeasureText(dimensions, uiSize(12)),
             margin + uiSize(13), uiSize(12), Accent);
    DrawRectangle(x, y, board, board, Ink);
    for (int row = 0; row < size; ++row)
        for (int col = 0; col < size; ++col) {
            int seen = game.seenState(row, col);
            if (!seen)
                continue;
            char tile = game.maze().cell(row, col);
            Color color = tile == '#'   ? MazeTop
                          : tile == 'E' ? Secondary
                          : seen == 2   ? MapVisited
                                        : MazeFloor;
            DrawRectangle(x + col * cell, y + row * cell, cell, cell, color);
        }
    float px = x + (game.x() / MazeGeometry::CellSize + .5f) * cell;
    float py = y + (game.z() / MazeGeometry::CellSize + .5f) * cell;
    float radius = std::max(2.0f, cell * .48f);
    DrawCircleV({px, py}, radius, Accent);
    DrawLineEx({px, py},
               {px + std::cos(game.yaw()) * radius * 3, py + std::sin(game.yaw()) * radius * 3},
               std::max(1.0f, uiScale()), Paper);
    const char* caption = "EXPLORED AREA";
    // Respect DrawText's ten-pixel minimum font size and switch to a shorter caption when the
    // minimap label would overflow.
    int captionSize = std::max(10, uiSize(10));
    if (MeasureText(caption, captionSize) > board)
        caption = "MAP";
    DrawText(caption, x, y + board + uiSize(12), captionSize, Muted);
}
