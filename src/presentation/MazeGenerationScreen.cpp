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

    return elapsed >= BUILD_DURATION || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

void MazeGenerationScreen::draw(const Game& game) const {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int mazeSize = game.maze().size();
    float progress = std::min(elapsed / BUILD_DURATION, 1.0f);
    int cellSize = (int)std::floor(std::min((width - 100.0f) / mazeSize,
                                           (height - 210.0f) / mazeSize));
    if (cellSize < 2) cellSize = 2;
    int boardSize = cellSize * mazeSize;
    int originX = (width - boardSize) / 2;
    int originY = (height - boardSize) / 2 + 8;

    ClearBackground(Color{12, 8, 28, 255});
    DrawRectangleGradientV(0, 0, width, height,
                           Color{45, 15, 82, 255}, Color{8, 6, 22, 255});
    drawCentered(TextFormat("BUILDING %d x %d MAZE", mazeSize, mazeSize), 28, 32, GOLD);

    DrawRectangle(originX - 5, originY - 5, boardSize + 10, boardSize + 10,
                  Color{7, 5, 20, 255});
    for (int row = 0; row < mazeSize; row++) {
        for (int col = 0; col < mazeSize; col++) {
            int x = originX + col * cellSize;
            int y = originY + row * cellSize;
            Color color = ((row + col) % 2 == 0)
                ? Color{31, 23, 48, 255}
                : Color{25, 19, 41, 255};

            if (revealed[row * mazeSize + col]) {
                color = Color{119, 36, 118, 255};
                if (row == 0 && col == 0) color = GOLD;
                if (game.maze().cell(row, col) == 'E' && progress >= 1.0f)
                    color = Color{78, 235, 168, 255};
                if (shownSteps > 0 && progress < 1.0f) {
                    const MazeStep& current = game.maze().generationSteps()[shownSteps - 1];
                    if (current.row == row && current.col == col)
                        color = Color{255, 93, 195, 255};
                }
            }

            DrawRectangle(x, y, cellSize - 1, cellSize - 1, color);
        }
    }
    DrawRectangleLines(originX - 5, originY - 5, boardSize + 10, boardSize + 10,
                       Color{255, 75, 190, 220});

    int barWidth = width * 3 / 5;
    int barX = (width - barWidth) / 2;
    int barY = height - uiSize(72);
    int barHeight = uiSize(8);
    DrawRectangle(barX, barY, barWidth, barHeight, Color{38, 28, 58, 255});
    DrawRectangle(barX, barY, (int)(barWidth * progress), barHeight, Color{255, 63, 190, 255});
    drawCenteredAt(TextFormat("%d%%", (int)(progress * 100.0f)),
                   barY - uiSize(30), uiSize(20), WHITE);
    drawCenteredAt("ENTER / SPACE  SKIP", height - uiSize(42),
                   uiSize(18), Color{190, 180, 210, 255});
}
