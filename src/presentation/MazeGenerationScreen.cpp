#include "MazeGenerationScreen.h"
#include "Ui.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

static constexpr float BUILD_DURATION = 4.5f;

// Reset preview timing and allocate a hidden-cell mask matching the newly generated maze.
void MazeGenerationScreen::begin(const Game& game) {
    elapsed = 0;
    shownSteps = 0;
    const int size = game.maze().size();
    revealed.assign(size * size, 0);
}

// Advance only with elapsed frame time. Keyboard and pointer input cannot bypass generation.
bool MazeGenerationScreen::update(const Game& game) {
    const float delta = GetFrameTime();
    if (std::isfinite(delta) && delta > 0)
        elapsed = std::min(elapsed + delta, BUILD_DURATION);
    const std::vector<MazeStep>& steps = game.maze().generationSteps();
    const float progress = elapsed / BUILD_DURATION;
    const int targetSteps = (int)(progress * steps.size());
    while (shownSteps < targetSteps) {
        const MazeStep& step = steps[shownSteps++];
        revealed[step.row * game.maze().size() + step.col] = 1;
    }
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    return elapsed >= BUILD_DURATION && shownSteps == (int)steps.size();
}

// Show the actual maze being carved in the same visual language as profile and level selection.
void MazeGenerationScreen::draw(const Game& game) const {
    using namespace MenuTheme;
    const int size = game.maze().size();
    const float progress = elapsed / BUILD_DURATION;
    background.draw((float)GetTime(), -0.16f);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), uiY(190), Ink, Fade(Ink, 0));
    drawMenuHeading(type, "03 / MAZE", "Generating maze");

    const Vector2 emblem = {(float)uiX(222), (float)uiY(332)};
    DrawPoly(emblem, 6, 94 * uiScale(), 30, Panel);
    DrawPolyLinesEx(emblem, 6, 94 * uiScale(), 30, std::max(1.0f, uiScale()), Border);
    DrawPoly(emblem, 6, 80 * uiScale(), 30, Selected);
    DrawPolyLinesEx(emblem, 6, 80 * uiScale(), 30, std::max(1.5f, 2 * uiScale()), Accent);
    type.centeredIn(
        TextFormat("%02d", (int)(progress * 100)),
        {emblem.x - 80 * uiScale(), emblem.y - 80 * uiScale(), 160 * uiScale(), 160 * uiScale()},
        68, Paper);
    type.centered("%", 222, 378, 15, Accent, true);
    type.centered(TextFormat("%d x %d cells", size, size), 222, 433, 25, Paper, true, 316);
    type.centered(game.mode() == GameMode::ZeroGravity ? "ZERO GRAVITY" : "NORMAL GRAVITY", 222,
                  474, 13, game.mode() == GameMode::ZeroGravity ? Secondary : Muted, true, 316);
    DrawRectangle(uiX(92), uiY(528), uiSize(260), std::max(3, uiSize(5)), Border);
    DrawRectangle(uiX(92), uiY(528), (int)(uiSize(260) * progress), std::max(3, uiSize(5)), Accent);
    type.centered("Starting automatically when ready", 222, 554, 14, Muted, false, 316);

    drawMenuPanel(uiRect(430, 210, 506, 380));
    type.draw("MAZE PREVIEW", 454, 230, 13, Muted, true);
    type.centered(TextFormat("%d x %d", size, size), 881, 230, 13, Accent, true, 76);
    DrawLine(uiX(454), uiY(262), uiX(912), uiY(262), Border);
    const int cell = std::max(1, uiSize(292) / size);
    const int board = cell * size;
    const int originX = uiX(683) - board / 2, originY = uiY(425) - board / 2;
    DrawRectangle(originX - 6, originY - 6, board + 12, board + 12, Ink);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            Color color = Ink;
            if (revealed[row * size + col]) {
                color = MazeTop;
                if (row == 0 && col == 0)
                    color = Accent;
                if (game.maze().cell(row, col) == 'E' && progress >= 1)
                    color = Accent;
                if (shownSteps > 0 && progress < 1) {
                    const MazeStep& current = game.maze().generationSteps()[shownSteps - 1];
                    if (current.row == row && current.col == col)
                        color = Accent;
                }
            }
            DrawRectangle(originX + col * cell, originY + row * cell, std::max(1, cell - 1),
                          std::max(1, cell - 1), color);
        }
    }
    DrawRectangleLines(originX - 6, originY - 6, board + 12, board + 12, Border);
    drawMenuFooter(type, "The timer starts when the maze is ready.");
}
