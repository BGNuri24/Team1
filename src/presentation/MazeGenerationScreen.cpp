#include "MazeGenerationScreen.h"
#include "Ui.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

static constexpr float BUILD_DURATION = 4.5f;

namespace {
void planRule(int x1, int y1, int x2, int y2, Color color, float width = 1) {
    DrawLineEx({(float)uiX(x1), (float)uiY(y1)}, {(float)uiX(x2), (float)uiY(y2)},
               std::max(1.0f, width * uiScale()), color);
}

// The pen is a foreground drawing tool; its position follows carving without changing the maze.
void planPen(Vector2 tip, Color ink, Color brass) {
    const float s = uiScale();
    rlPushMatrix();
    rlTranslatef(tip.x + 3 * s, tip.y + 5 * s, 0);
    rlRotatef(32, 0, 0, 1);
    DrawRectangleRounded({-4 * s, -104 * s, 8 * s, 91 * s}, .7f, 8, Fade(ink, .18f));
    rlPopMatrix();
    rlPushMatrix();
    rlTranslatef(tip.x, tip.y, 0);
    rlRotatef(32, 0, 0, 1);
    DrawRectangleRounded({-4 * s, -104 * s, 8 * s, 91 * s}, .7f, 8, ink);
    DrawRectangleRec({-4 * s, -91 * s, 8 * s, 3 * s}, brass);
    DrawRectangleRec({1 * s, -85 * s, 2 * s, 36 * s}, brass);
    DrawRectangleRec({-3 * s, -18 * s, 6 * s, 6 * s}, brass);
    DrawTriangle({0, 0}, {3 * s, -12 * s}, {-3 * s, -12 * s}, brass);
    DrawLineEx({0, -8 * s}, {0, -2 * s}, std::max(1.0f, s), ink);
    rlPopMatrix();
}
} // namespace

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

// Draw the selected maze as an architectural ink plan, with a moving fountain pen and drafting
// rulers. The sheet shows only revealed passages; the mandatory generation timing stays separate.
void MazeGenerationScreen::draw(const Game& game) const {
    using namespace MenuTheme;
    const int size = game.maze().size();
    const float progress = std::clamp(elapsed / BUILD_DURATION, 0.0f, 1.0f);
    const float s = uiScale();
    const Color sheet = ColorLerp(Paper, Accent, .08f);
    const Color ink = ColorLerp(Ink, Panel, .25f);
    const Color grid = Fade(ink, .055f);
    background.draw((float)GetTime());

    DrawRectangleRec(uiRect(177, 91, 668, 548), Fade(Ink, .6f));
    DrawRectangleRec(uiRect(166, 76, 668, 548), ColorLerp(sheet, Accent, .24f));
    DrawRectangleRec(uiRect(178, 76, 644, 548), sheet);
    DrawRectangleLinesEx(uiRect(178, 76, 644, 548), std::max(1.0f, s), ColorLerp(sheet, ink, .24f));
    for (int x = 194; x < 814; x += 16)
        planRule(x, 88, x, 612, grid);
    for (int y = 92; y < 616; y += 16)
        planRule(186, y, 814, y, grid);
    planRule(174, 80, 174, 620, Fade(ink, .16f));
    planRule(826, 80, 826, 620, Fade(ink, .16f));

    type.draw(progress < 1 ? "Drawing" : "Ready", 198, 94, 23, ink, true);
    type.draw(TextFormat("%d%%", (int)std::round(progress * 100)), 738, 94, 26, ink, true, 68);
    planRule(198, 134, 802, 134, Fade(ink, .25f));

    const float board = (float)uiSize(400);
    const float cell = board / size;
    const float originX = (float)uiX(300), originY = (float)uiY(156);
    const float stroke = std::max(1.0f, .9f * s);
    auto visible = [&](int row, int col) {
        return row >= 0 && row < size && col >= 0 && col < size &&
               (int)revealed.size() == size * size && revealed[row * size + col] != 0;
    };
    // Light reference ticks preserve scale without numbering every corridor.
    for (int tick = 0; tick <= size; ++tick) {
        const float x = std::round(originX + tick * cell);
        const float y = std::round(originY + tick * cell);
        const float length = (tick % 5 == 0 ? 6 : 3) * s;
        DrawLineEx({x, originY - 6 * s}, {x, originY - (6 * s + length)}, stroke, Fade(ink, .35f));
        DrawLineEx({originX - 6 * s, y}, {originX - (6 * s + length), y}, stroke, Fade(ink, .35f));
    }

    const Color passage = ColorLerp(sheet, Accent, .20f);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (!visible(row, col))
                continue;
            const float x = std::round(originX + col * cell);
            const float y = std::round(originY + row * cell);
            const float right = std::round(originX + (col + 1) * cell);
            const float bottom = std::round(originY + (row + 1) * cell);
            DrawRectangleRec({x, y, right - x, bottom - y}, passage);
        }
    }
    // Outline the passage union instead of drawing disconnected square tiles.
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (!visible(row, col))
                continue;
            const float x = std::round(originX + col * cell);
            const float y = std::round(originY + row * cell);
            const float right = std::round(originX + (col + 1) * cell);
            const float bottom = std::round(originY + (row + 1) * cell);
            if (!visible(row - 1, col))
                DrawLineEx({x, y}, {right, y}, stroke, ink);
            if (!visible(row + 1, col))
                DrawLineEx({x, bottom}, {right, bottom}, stroke, ink);
            if (!visible(row, col - 1))
                DrawLineEx({x, y}, {x, bottom}, stroke, ink);
            if (!visible(row, col + 1))
                DrawLineEx({right, y}, {right, bottom}, stroke, ink);
        }
    }

    // Registration marks and a set square belong to the drafting sheet, not the playable map.
    for (int corner = 0; corner < 4; ++corner) {
        const float x = originX + (corner % 2 ? board : 0);
        const float y = originY + (corner / 2 ? board : 0);
        const float dx = corner % 2 ? 1.0f : -1.0f;
        const float dy = corner / 2 ? 1.0f : -1.0f;
        DrawLineEx({x + dx * 9 * s, y}, {x + dx * 17 * s, y}, stroke, Fade(ink, .4f));
        DrawLineEx({x, y + dy * 9 * s}, {x, y + dy * 17 * s}, stroke, Fade(ink, .4f));
    }
    const Color tool = Fade(ink, .33f);
    planRule(204, 482, 204, 546, tool);
    planRule(204, 546, 268, 546, tool);
    planRule(268, 546, 204, 482, tool);
    planRule(215, 509, 215, 535, tool);
    planRule(215, 535, 241, 535, tool);
    planRule(241, 535, 215, 509, tool);
    for (int tick = 0; tick < 6; ++tick)
        planRule(204, 492 + tick * 9, tick % 2 == 0 ? 210 : 207, 492 + tick * 9, tool);

    planRule(198, 582, 802, 582, Fade(ink, .25f));
    type.draw(TextFormat("%d x %d", size, size), 198, 592, 19, ink, true);
    if (game.mode() == GameMode::ZeroGravity)
        type.draw("Zero gravity", 684, 595, 14, Fade(ink, .7f), false, 118);

    // A sliding T-square and the nib follow the active carving cell. Long frontier jumps lift
    // the pen rather than drawing a false connecting corridor.
    const auto& steps = game.maze().generationSteps();
    if (!steps.empty()) {
        const int current = std::clamp(shownSteps - 1, 0, (int)steps.size() - 1);
        const int next = std::min(shownSteps, (int)steps.size() - 1);
        float row = (float)steps[current].row, col = (float)steps[current].col;
        if (progress < 1 && std::abs(steps[current].row - steps[next].row) +
                                    std::abs(steps[current].col - steps[next].col) ==
                                1) {
            const float fraction = progress * steps.size() - std::floor(progress * steps.size());
            row += (steps[next].row - row) * fraction;
            col += (steps[next].col - col) * fraction;
        }
        const Vector2 tip = {originX + (col + .5f) * cell, originY + (row + .5f) * cell};
        if (progress < 1) {
            DrawRectangleRec({(float)uiX(151), tip.y - 5 * s, 18 * s, 10 * s}, Panel);
            DrawRectangleRec({(float)uiX(162), tip.y - 2 * s, 560 * s, 3 * s},
                             ColorLerp(Accent, Panel, .22f));
            for (int tick = 0; tick < 56; ++tick) {
                const float x = uiX(168) + tick * 10 * s;
                DrawLineEx({x, tip.y - 2 * s}, {x, tip.y - (tick % 5 == 0 ? 8 : 5) * s}, stroke,
                           Fade(ink, .35f));
            }
            planPen(tip, ink, ColorLerp(Accent, Panel, .3f));
        } else
            planPen({(float)uiX(764), (float)uiY(536)}, ink, ColorLerp(Accent, Panel, .3f));
    }
}
