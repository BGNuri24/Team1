#include "EndScreen.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

// Format a nonnegative duration as minutes, seconds and one decimal digit using raylib's temporary
// text buffer.
static const char* formatTime(float t) {
    int minutes = (int)t / 60;
    int seconds = (int)t % 60;
    int tenths = (int)(t * 10) % 10;
    return TextFormat("%02d:%02d.%d", minutes, seconds, tenths);
}

void EndScreen::reset() {
    animationTime = 0.0f;
}

// Advance the non-blocking result animation and wait for confirmation, Escape or the report button.
Screen EndScreen::update(float deltaSeconds) {
    if (std::isfinite(deltaSeconds) && deltaSeconds > 0.0f)
        animationTime += std::min(deltaSeconds, 0.1f);
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE) || menuClicked(uiRect(330, 504, 340, 48)))
        return Screen::MAIN_MENU;
    return Screen::END;
}

// Overlay the run outcome, elapsed and remaining time, score, and the active player's best result on
// the finished maze.
void EndScreen::draw(const Game& game, const std::string& playerName,
    float bestTime, bool newBestTime, bool bestTimeSaveFailed) const {
    using namespace MenuTheme;
    const Color accent = game.won() ? Mint : Danger;
    const float pulse = 0.5f + 0.5f * std::sin(animationTime * 4.0f);
    const int iconX = uiX(243);
    const int iconY = uiY(239);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, 0.83f));
    DrawCircleLines(iconX, iconY, (float)uiSize((int)(38 + pulse * 8.0f)), Fade(accent, 0.2f + pulse * 0.2f));
    DrawCircleLines(iconX, iconY, (float)uiSize((int)(48 + pulse * 12.0f)), Fade(accent, 0.08f + pulse * 0.1f));
    for (int i = 0; i < 10; ++i) {
        const float angle = animationTime * (0.8f + i * 0.03f) + i * 0.6283185f;
        const float distance = (float)uiSize((int)(58.0f + std::fmod(animationTime * 24.0f + i * 13.0f, 34.0f)));
        const int particleX = iconX + (int)(std::cos(angle) * distance);
        const int particleY = iconY + (int)(std::sin(angle) * distance);
        DrawCircle(particleX, particleY, (float)uiSize(2 + (i % 2)), Fade(accent, 0.18f + pulse * 0.35f));
    }
    drawMenuPanel(uiRect(176, 125, 648, 460), Fade(accent, 0.6f));
    drawMenuText("EXPEDITION REPORT", 212, 158, 14, accent);
    DrawRectangleLinesEx(uiRect(218, 209, 50, 60), 3*uiScale(), accent);
    DrawLineEx({(float)iconX, (float)iconY}, {(float)uiX(287), (float)uiY(239)}, 3*uiScale(), accent);
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
    drawCentered(bestTimeSaveFailed ? "Best time could not be saved. Check save_data access." : record,
        456, 16, bestTimeSaveFailed ? Danger : newBestTime ? Amber : Muted);
    drawMenuButton(uiRect(330, 504, 340, 48), "Back to main menu", true);
    drawCentered("ENTER / ESC  continue", 603, 14, Muted);
}
