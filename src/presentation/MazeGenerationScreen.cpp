#include "MazeGenerationScreen.h"
#include "Ui.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

static const float BUILD_DURATION = 4.5f;

void MazeGenerationScreen::begin(const Game& game) {
    elapsed = 0.0f;
    shownSteps = 0;
    int cellCount = game.maze().size();
    revealed.assign(cellCount * cellCount, 0);
}

bool MazeGenerationScreen::update(const Game& game) {
    elapsed += GetFrameTime();
    const std::vector<MazeStep>& steps = game.maze().generationSteps();
    float progress = std::min(elapsed / BUILD_DURATION, 1.0f);
    int targetSteps = (int)(progress * steps.size());
    while (shownSteps < targetSteps) {
        const MazeStep& step = steps[shownSteps++];
        revealed[step.row * game.maze().size() + step.col] = 1;
    }

    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    return elapsed >= BUILD_DURATION || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)
        || menuClicked(uiRect(774, 631, 162, 38));
}

void MazeGenerationScreen::draw(const Game& game) const {
    using namespace MenuTheme;
    int mazeSize = game.maze().size();
    float progress = std::min(elapsed / BUILD_DURATION, 1.0f);
    int cellSize = (int)std::floor(uiSize(366) / (float)mazeSize);
    if (cellSize < 2) cellSize = 2;
    int boardSize = cellSize * mazeSize;
    int originX = uiX(720) - boardSize / 2;
    int originY = uiY(404) - boardSize / 2;

    drawMenuBackdrop(4);
    drawMenuHeading("03 / CARVING THE CORRIDORS", "A world taking shape.", "No two journeys need to take the same turns.");
    drawMenuPanel(uiRect(64, 210, 414, 380));
    drawMenuText("GENERATING YOUR MAZE", 90, 240, 14, Amber);
    drawMenuText(TextFormat("%02d", (int)(progress * 100)), 86, 285, 100);
    drawMenuText("%", 290, 339, 40, Amber);
    drawMenuText(TextFormat("%d x %d cells", mazeSize, mazeSize), 90, 414, 30);
    drawMenuText(game.mode() == GameMode::ZeroGravity ? "ZERO GRAVITY" : "NORMAL GRAVITY", 90, 458, 14, Mint);
    drawMenuText("The green marker is your way out.", 90, 514, 16, Muted);
    drawMenuText("Get your bearings before the clock starts.", 90, 544, 14, Muted);
    drawMenuPanel(uiRect(504, 192, 432, 416));
    DrawRectangle(originX - 5, originY - 5, boardSize + 10, boardSize + 10,
        Ink);
    for (int row = 0; row < mazeSize; row++) {
        for (int col = 0; col < mazeSize; col++) {
            int x = originX + col * cellSize;
            int y = originY + row * cellSize;
            Color color = ((row + col) % 2 == 0)
                ? Color{ 26, 49, 55, 255 }
            : Color{ 23, 44, 50, 255 };

            if (revealed[row * mazeSize + col]) {
                color = Color{ 62, 116, 115, 255 };
                if (row == 0 && col == 0) color = Amber;
                if (game.maze().cell(row, col) == 'E' && progress >= 1.0f)
                    color = Mint;
                if (shownSteps > 0 && progress < 1.0f) {
                    const MazeStep& current = game.maze().generationSteps()[shownSteps - 1];
                    if (current.row == row && current.col == col)
                        color = Amber;
                }
            }

            DrawRectangle(x, y, cellSize - 1, cellSize - 1, color);
        }
    }
    DrawRectangleLines(originX - 5, originY - 5, boardSize + 10, boardSize + 10,
        Border);

    DrawRectangle(uiX(90), uiY(389), uiSize(358), uiSize(5), Border);
    DrawRectangle(uiX(90), uiY(389), (int)(uiSize(358) * progress), uiSize(5), Amber);
    drawMenuFooter("THE PATH IS BEING CARVED.     ENTER / SPACE  skip");
    drawMenuButton(uiRect(774, 631, 162, 38), "Skip", false);
}
