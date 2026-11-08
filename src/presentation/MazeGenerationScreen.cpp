#include "MazeGenerationScreen.h"
#include "Ui.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

static constexpr float BUILD_DURATION = 4.5f;

namespace {
void directoryRule(int x1, int y1, int x2, int y2, Color color) {
    DrawLineEx({(float)uiX(x1), (float)uiY(y1)}, {(float)uiX(x2), (float)uiY(y2)},
               std::max(1.0f, uiScale()), color);
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

// Reveal the selected maze inside a walnut and brass hotel floor directory.
// Drawing follows the existing carving mask; it does not advance generation or the run timer.
void MazeGenerationScreen::draw(const Game& game) const {
    using namespace MenuTheme;
    const int size = game.maze().size();
    const float progress = std::clamp(elapsed / BUILD_DURATION, 0.0f, 1.0f);
    const float s = uiScale();
    const Color wood = ColorLerp(Panel, Ink, .28f);
    const Color bronze = ColorLerp(Accent, Panel, .50f);
    const Color face = ColorLerp(Ink, Panel, .22f);
    const Color well = ColorLerp(Ink, Panel, .06f);
    const Color lettering = ColorLerp(Paper, Accent, .30f);
    const Color line = ColorLerp(bronze, Paper, .10f);
    const float stroke = std::max(1.0f, s);

    background.draw((float)GetTime());
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .18f));

    const Rectangle frame = uiRect(205, 64, 590, 578);
    DrawRectangleRec(uiRect(213, 75, 590, 578), Fade(Ink, .65f));
    DrawRectangleGradientV((int)frame.x, (int)frame.y, (int)frame.width, (int)frame.height, wood,
                           ColorLerp(wood, Ink, .60f));
    DrawRectangleLinesEx(frame, stroke, ColorLerp(wood, bronze, .30f));
    DrawRectangleRec(uiRect(218, 77, 564, 552), face);
    DrawRectangleLinesEx(uiRect(214, 73, 572, 560), stroke, bronze);
    DrawRectangleLinesEx(uiRect(219, 78, 562, 550), stroke, Fade(bronze, .28f));

    // Mitered frame edges and flush brass fixings keep the finish architectural.
    directoryRule(205, 64, 218, 77, Fade(bronze, .45f));
    directoryRule(795, 64, 782, 77, Fade(bronze, .45f));
    directoryRule(205, 642, 218, 629, Fade(bronze, .24f));
    directoryRule(795, 642, 782, 629, Fade(bronze, .24f));
    for (int x : {211, 789})
        for (int y : {70, 636})
            DrawCircleV({(float)uiX(x), (float)uiY(y)}, std::max(1.0f, 1.3f * s), bronze);

    type.draw("FLOOR PLAN", 244, 96, 19, lettering, true, 300);
    type.centered(TextFormat("%d x %d", size, size), 714, 99, 15, line, true, 84);
    directoryRule(244, 135, 756, 135, Fade(bronze, .60f));

    const Rectangle inset = uiRect(284, 153, 432, 432);
    DrawRectangleGradientV((int)inset.x, (int)inset.y, (int)inset.width, (int)inset.height, well,
                           ColorLerp(well, face, .35f));
    DrawRectangleLinesEx(inset, stroke, Fade(bronze, .38f));

    const float board = (float)uiSize(400);
    const float cell = board / size;
    const float originX = (float)uiX(300), originY = (float)uiY(169);
    auto visible = [&](int row, int col) {
        return row >= 0 && row < size && col >= 0 && col < size &&
               (int)revealed.size() == size * size && revealed[row * size + col] != 0;
    };

    const Color passage = ColorLerp(well, bronze, .18f);
    const auto& steps = game.maze().generationSteps();
    const int current = shownSteps > 0 ? shownSteps - 1 : -1;
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (!visible(row, col))
                continue;
            const float x = std::round(originX + col * cell);
            const float y = std::round(originY + row * cell);
            const float right = std::round(originX + (col + 1) * cell);
            const float bottom = std::round(originY + (row + 1) * cell);
            const bool active = progress < 1 && current >= 0 && current < (int)steps.size() &&
                                steps[current].row == row && steps[current].col == col;
            DrawRectangleRec({x, y, right - x, bottom - y},
                             active ? ColorLerp(passage, bronze, .26f) : passage);
        }
    }

    // Continuous passage edges read as an engraved plan, including on the largest grids.
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            if (!visible(row, col))
                continue;
            const float x = std::round(originX + col * cell);
            const float y = std::round(originY + row * cell);
            const float right = std::round(originX + (col + 1) * cell);
            const float bottom = std::round(originY + (row + 1) * cell);
            if (!visible(row - 1, col))
                DrawLineEx({x, y}, {right, y}, stroke, line);
            if (!visible(row + 1, col))
                DrawLineEx({x, bottom}, {right, bottom}, stroke, line);
            if (!visible(row, col - 1))
                DrawLineEx({x, y}, {x, bottom}, stroke, line);
            if (!visible(row, col + 1))
                DrawLineEx({right, y}, {right, bottom}, stroke, line);
        }
    }

    type.draw(progress < 1 ? "Preparing" : "Ready", 244, 601, 15, lettering, true, 170);
    if (game.mode() == GameMode::ZeroGravity)
        type.centered("Zero gravity", 500, 603, 13, line, true, 160);
    type.centered(TextFormat("%d%%", (int)std::round(progress * 100)), 718, 592, 26, lettering,
                  true, 80);
    const Rectangle track = uiRect(244, 627, 512, 2);
    DrawRectangleRec(track, ColorLerp(face, bronze, .20f));
    DrawRectangleRec({track.x, track.y, track.width * progress, track.height}, bronze);
}
