#include "HudRenderer.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>
namespace {
struct HudTheme {
    Color panel, border, text, muted, accent, track, warning;
    Color unknown, walls, visible, visited, marker;
};

// Derive readable HUD material roles from the loaded run's palette independently of the current
// menu theme. This pure conversion avoids retaining colors from a previous run or consuming
// random state.
HudTheme hudTheme(const MazePalette& palette) {
    const MenuTheme::Colors colors = MenuTheme::colorsFor(palette);
    return {colors.panel,   colors.border,     colors.paper,      colors.muted,
            colors.accent,  colors.track,      colors.danger,     colors.mapUnknown,
            colors.mapWall, colors.mapVisible, colors.mapVisited, colors.marker};
}

// Draw the map's panel frame while leaving its canvas responsible for the translucent world
// overlay. Keeping the tint in one layer avoids unintentionally darkening the scene twice.
void hudPanel(Rectangle bounds, Rectangle canvas, const HudTheme& colors) {
    const Rectangle strips[] = {{bounds.x, bounds.y, bounds.width, canvas.y - bounds.y},
                                {bounds.x, canvas.y + canvas.height, bounds.width,
                                 bounds.y + bounds.height - canvas.y - canvas.height},
                                {bounds.x, canvas.y, canvas.x - bounds.x, canvas.height},
                                {canvas.x + canvas.width, canvas.y,
                                 bounds.x + bounds.width - canvas.x - canvas.width, canvas.height}};
    for (const Rectangle strip : strips) {
        BeginScissorMode((int)strip.x, (int)strip.y, (int)strip.width, (int)strip.height);
        DrawRectangleRounded(bounds, 0.10f, 8, Fade(colors.panel, .85f));
        EndScissorMode();
    }
    DrawRectangleRoundedLinesEx(bounds, 0.10f, 8, std::max(1.0f, uiScale()), colors.border);
}

// Draw a compact map-key hint while the map is hidden, with a ring indicating cooldown progress.
// Read MapPeek timing without advancing it so rendering cannot recharge the ability.
void mapKey(const MapPeek& peek, const HudTheme& colors) {
    const int margin = uiSize(22), radius = std::max(12, uiSize(15));
    const Vector2 center = {(float)(GetScreenWidth() - margin - radius), (float)(margin + radius)};
    DrawCircleLinesV(center, (float)radius, Fade(colors.accent, .20f));
    if (peek.ready())
        DrawCircleLinesV(center, (float)radius, Fade(colors.accent, .8f));
    else {
        const float progress =
            std::clamp(1 - peek.cooldownTimeLeft() / MapPeek::CooldownSeconds, 0.0f, 1.0f);
        if (progress > 0)
            DrawRing(center, radius - std::max(1.0f, uiScale()), (float)radius, -90,
                     -90 + progress * 360, 64, Fade(colors.accent, .8f));
    }
    const int size = std::max(10, uiSize(15));
    DrawText("E", (int)center.x - MeasureText("E", size) / 2, (int)center.y - size / 2, size,
             Fade(colors.text, peek.ready() ? .8f : .35f));
}
} // namespace
// Compose the timer and optional map UI in screen space after world rendering has finished. The
// caller controls whether gameplay supplies an active peek state or an ordinary hidden-map hint.
void HudRenderer::draw(const Game& game, const MazePalette& palette, const MapPeek* mapPeek) const {
    drawHud(game, palette, !mapPeek || !mapPeek->visible());
    if (mapPeek) {
        if (mapPeek->visible())
            drawMinimap(game, palette);
        else
            mapKey(*mapPeek, hudTheme(palette));
    }
}
// Draw the centered translucent countdown digits over the world without an opaque panel or
// compass. Read the Game timer only; paused and finished frames must not advance it.
void HudRenderer::drawHud(const Game& game, const MazePalette& palette, bool showReticle) const {
    using namespace MenuTheme;
    const HudTheme colors = hudTheme(palette);
    const int margin = uiSize(22);
    // Measure the label and draw it using the HUD's requested reference position, size, and
    // color. This local callback keeps repeated HUD text aligned with the same scaling rules.
    auto text = [&](const char* label, int x, int y, int size, Color color) {
        DrawText(label, x, y, std::max(1, uiSize(size)), color);
    };
    const int remaining = std::max(0, (int)std::ceil(game.timeLeft()));
    const Color clockColor = remaining <= 30 ? colors.warning : colors.text;
    const char* clock = TextFormat("%02d:%02d", remaining / 60, remaining % 60);
    const int clockSize = std::max(10, uiSize(42));
    const int clockX = (GetScreenWidth() - MeasureText(clock, clockSize)) / 2;
    const int clockY = margin + uiSize(10);
    const int shadow = std::max(1, uiSize(1));
    DrawText(clock, clockX + shadow, clockY + shadow, clockSize, Fade(colors.unknown, .32f));
    DrawText(clock, clockX, clockY, clockSize, Fade(clockColor, .78f));
    const bool zero = game.mode() == GameMode::ZeroGravity;

    const int footer = GetScreenHeight() - uiSize(40);
    text("ESC  pause", margin, footer, 15, OnWorld);
    if (zero)
        text(game.surfaceAttached() ? "G  return to floor" : "G  switch to ceiling", margin,
             footer - uiSize(28), 15, ColorLerp(Secondary, OnWorld, .65f));
    if (zero)
        text("RED BARRIERS ARE LETHAL", margin, footer - uiSize(52), 13,
             ColorLerp(Danger, OnWorld, .65f));
    // Keep the world reticle from showing through the map and resembling a second player marker.
    if (!showReticle)
        return;
    // Keep a central reticle as a stable reference while the camera rolls between surfaces.
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    Color aim = Fade(OnWorld, .8f);
    DrawLine(cx - uiSize(5), cy, cx - uiSize(2), cy, aim);
    DrawLine(cx + uiSize(2), cy, cx + uiSize(5), cy, aim);
    DrawLine(cx, cy - uiSize(5), cx, cy - uiSize(2), aim);
    DrawLine(cx, cy + uiSize(2), cx, cy + uiSize(5), aim);
}

// Draw a centered enlarged peek using only the run's revealed cells, keeping the timer and window
// edges clear. Showing a larger map never discloses unexplored passages or changes exploration
// state.
void HudRenderer::drawMinimap(const Game& game, const MazePalette& palette) const {
    const HudTheme colors = hudTheme(palette);
    const int size = game.maze().size();
    const int available =
        std::max(1, std::min({(int)(std::min(GetScreenWidth(), GetScreenHeight()) * .62f),
                              GetScreenWidth() - uiSize(80), GetScreenHeight() - uiSize(160)}));
    const int cell = std::max(1, available / size), board = size * cell;
    const int padding = std::max(12, uiSize(20));
    const int header = std::max(30, uiSize(40)), footer = std::max(26, uiSize(34));
    const int panelWidth = board + padding * 2, panelHeight = board + header + footer;
    const int panelX = (GetScreenWidth() - panelWidth) / 2;
    const int panelY = (GetScreenHeight() - panelHeight) / 2;
    const Rectangle panel = {(float)panelX, (float)panelY, (float)panelWidth, (float)panelHeight};
    const int x = panelX + padding, y = panelY + header;
    DrawRectangleRoundedLinesEx(
        {panel.x + uiSize(3), panel.y + uiSize(4), panel.width, panel.height}, .10f, 8,
        std::max(1.0f, 2 * uiScale()), Fade(colors.unknown, .35f));
    hudPanel(panel, {(float)x, (float)y, (float)board, (float)board}, colors);
    const int labelSize = std::max(12, uiSize(14));
    const int labelY = panelY + (header - labelSize) / 2;
    DrawText("MAP", x, labelY, labelSize, colors.muted);
    const char* dimensions = TextFormat("%d x %d", size, size);
    DrawText(dimensions, x + board - MeasureText(dimensions, labelSize), labelY, labelSize,
             colors.accent);
    BeginScissorMode(x, y, board, board);
    for (int row = 0; row < size; ++row)
        for (int col = 0; col < size; ++col) {
            int seen = game.seenState(row, col);
            char tile = game.maze().cell(row, col);
            Color color = !seen         ? colors.unknown
                          : tile == '#' ? colors.walls
                          : tile == 'E' ? colors.marker
                          : seen == 2   ? colors.visited
                                        : colors.visible;
            DrawRectangle(x + col * cell, y + row * cell, cell, cell,
                          Fade(color, seen ? .90f : .82f));
        }
    float px = x + (game.x() / MazeGeometry::CellSize + .5f) * cell;
    float py = y + (game.z() / MazeGeometry::CellSize + .5f) * cell;
    const float radius = std::clamp(cell * .35f, 2.0f, std::max(3.0f, 7 * uiScale()));
    DrawCircleV({px, py}, radius, colors.marker);
    DrawLineEx({px, py},
               {px + std::cos(game.yaw()) * radius * 3, py + std::sin(game.yaw()) * radius * 3},
               std::max(1.0f, uiScale()), colors.text);
    EndScissorMode();
    const char* caption = "EXPLORED AREA";
    // Respect DrawText's ten-pixel minimum font size and switch to a shorter caption when the
    // minimap label would overflow.
    int captionSize = std::max(10, uiSize(10));
    if (MeasureText(caption, captionSize) > board)
        caption = "MAP";
    DrawText(caption, x, y + board + (footer - captionSize) / 2, captionSize, colors.muted);
}
