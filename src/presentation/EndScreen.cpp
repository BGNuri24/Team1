#include "EndScreen.h"
#include "Ui.h"

static const char* formatTime(float t) {
    int minutes = (int)t / 60;
    int seconds = (int)t % 60;
    int tenths = (int)(t * 10) % 10;
    return TextFormat("%02d:%02d.%d", minutes, seconds, tenths);
}

Screen EndScreen::update() {
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) return Screen::MAIN_MENU;
    return Screen::END;
}

void EndScreen::draw(const Game& game, const std::string& playerName,
    float bestTime, bool newBestTime) const {
    // dim the maze
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));

    int w = uiSize(600), h = uiSize(400);
    int x = (GetScreenWidth() - w) / 2;
    int y = (GetScreenHeight() - h) / 2;
    DrawRectangle(x, y, w, h, Color{ 25, 28, 38, 245 });
    DrawRectangleLines(x, y, w, h, game.won() ? GOLD : RED);

    if (game.won()) {
        drawCenteredAt("FINISH LINE REACHED!", y + uiSize(25), uiSize(48), GOLD);
        drawCenteredAt(TextFormat("Player: %s", playerName.c_str()),
            y + uiSize(95), uiSize(30), SKYBLUE);
        drawCenteredAt(TextFormat("Time: %s", formatTime(game.timeTaken())),
            y + uiSize(150), uiSize(36), WHITE);
        drawCenteredAt(TextFormat("Time left: %s", formatTime(game.timeLeft())),
            y + uiSize(200), uiSize(30), LIGHTGRAY);
    }
    else {
        drawCenteredAt("TIME'S UP!", y + uiSize(25), uiSize(56), RED);
        drawCenteredAt(TextFormat("Player: %s", playerName.c_str()),
            y + uiSize(95), uiSize(30), SKYBLUE);
        drawCenteredAt("You did not reach the finish line",
            y + uiSize(160), uiSize(30), LIGHTGRAY);
    }
    const char* recordText = bestTime < 0.0f
        ? "Best time: --"
        : TextFormat("%s%s", newBestTime ? "NEW BEST: " : "Best time: ", formatTime(bestTime));
    drawCenteredAt(recordText, y + uiSize(245), uiSize(28), newBestTime ? GOLD : LIGHTGRAY);
    drawCenteredAt(TextFormat("Points: %d", game.score()), y + uiSize(290), uiSize(36), GOLD);
    drawCenteredAt("Press ENTER for the menu", y + uiSize(350), uiSize(24), GRAY);
}