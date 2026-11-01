#include "EndScreen.h"
#include "Ui.h"

// Format a nonnegative duration as minutes, seconds and one decimal digit using raylib's temporary
// text buffer.
static const char* formatTime(float t) {
    int minutes = (int)t / 60;
    int seconds = (int)t % 60;
    int tenths = (int)(t * 10) % 10;
    return TextFormat("%02d:%02d.%d", minutes, seconds, tenths);
}

// Wait for confirmation, Escape or the report button before returning to the main menu.
Screen EndScreen::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || menuClicked(uiRect(330, 504, 340, 48)))
        return Screen::MAIN_MENU;
    return Screen::END;
}

// Overlay the run outcome, elapsed and remaining time, score, and the active player's best result on
// the finished maze.
void EndScreen::draw(const Game& game, const std::string& playerName,
    float bestTime, bool newBestTime) const {
    using namespace MenuTheme;
    const Color accent = game.won() ? Mint : Danger;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, 0.83f));
    drawMenuPanel(uiRect(176, 125, 648, 460), Fade(accent, 0.6f));
    drawMenuText("EXPEDITION REPORT", 212, 158, 14, accent);
    DrawRectangleLinesEx(uiRect(218, 209, 50, 60), 3*uiScale(), accent);
    DrawLineEx({(float)uiX(243), (float)uiY(239)}, {(float)uiX(287), (float)uiY(239)}, 3*uiScale(), accent);
    if (game.won()) {
        DrawLineEx({(float)uiX(277), (float)uiY(229)}, {(float)uiX(287), (float)uiY(239)}, 3*uiScale(), accent);
        DrawLineEx({(float)uiX(277), (float)uiY(249)}, {(float)uiX(287), (float)uiY(239)}, 3*uiScale(), accent);
    }
    drawMenuText(game.won() ? "You found a way." : "The maze can wait.", 312, 206, 36);
    drawMenuTextFit(playerName.c_str(), 312, 255, 460, 22, Amber);
    drawMenuText(game.won() ? "Another labyrinth, another story." : "Time ran out. A fresh route is waiting.", 212, 295, 18, Muted);
    const int positions[] = {212, 410, 608};
    const char* labels[] = {game.won() ? "ESCAPE TIME" : "TIME IN MAZE", "TIME LEFT", "POINTS"};
    for (int i = 0; i < 3; ++i) {
        drawMenuPanel(uiRect(positions[i], 340, 180, 85));
        drawMenuText(labels[i], positions[i]+16, 357, 12, Muted);
        const char* value = i == 0 ? formatTime(game.timeTaken())
            : i == 1 ? formatTime(game.timeLeft()) : TextFormat("%d", game.score());
        drawMenuText(value, positions[i]+16, 385, 26, i == 2 ? Amber : Paper);
    }
    const char* record = bestTime < 0.0f ? "No escape recorded for this level yet."
        : TextFormat("%s   %s", newBestTime ? "A NEW PERSONAL BEST" : "PERSONAL BEST", formatTime(bestTime));
    drawCentered(record, 456, 18, newBestTime ? Amber : Muted);
    drawMenuButton(uiRect(330, 504, 340, 48), "Back to main menu", true);
    drawCentered("ENTER / ESC  continue", 603, 14, Muted);
}
