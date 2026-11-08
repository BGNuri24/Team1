#include "EndScreen.h"
#include "Input.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
Rectangle actionBounds(int index) {
    return uiRect(302 + index * 210, 576, 186, 48);
}

const char* formatTime(float value) {
    const int tenths = (int)(std::max(0.0f, value) * 10);
    return TextFormat("%02d:%02d.%d", tenths / 600, tenths / 10 % 60, tenths % 10);
}

void drawResultCard(float offsetX, float offsetY, Color color) {
    Rectangle body = uiRect(330, 224, 340, 312);
    body.x += offsetX;
    body.y += offsetY;
    DrawRectangleRounded(body, 0.06f, 8, color);
}
} // namespace

void EndScreen::reset() {
    animationTime = 0;
    selected = 0;
}

// Replay with the same settings, or leave the finished run.
Screen EndScreen::update(float deltaSeconds) {
    if (std::isfinite(deltaSeconds) && deltaSeconds > 0)
        animationTime += std::min(deltaSeconds, 0.1f);
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    updateMenuSelection(selected, 2);
    const bool left = IsKeyPressed(KEY_LEFT), right = IsKeyPressed(KEY_RIGHT);
    if (left != right)
        selected = 1 - selected;
    bool clicked = false;
    for (int index = 0; index < 2; ++index)
        clicked |= menuPointer(actionBounds(index), index, selected);
    if (IsKeyPressed(KEY_ESCAPE))
        return Screen::MAIN_MENU;
    if (menuConfirmed() || clicked)
        return selected == 0 ? Screen::PLAYING : Screen::MAIN_MENU;
    return Screen::END;
}

void EndScreen::draw(const Game& game, const std::string& playerName, float bestTime,
                     bool newBestTime, bool bestTimeSaveFailed) const {
    using namespace MenuTheme;
    const bool won = game.won();
    const bool hazard = game.failureReason() == FailureReason::ObstacleHit;
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, 0.68f));

    const float progress = std::clamp(animationTime / 0.4f, 0.0f, 1.0f);
    const float lift = -12 * (1 - progress) * (1 - progress) * uiScale();
    rlPushMatrix();
    rlTranslatef(0, lift, 0);
    drawResultCard(5 * uiScale(), 7 * uiScale(), Fade(Ink, 0.45f));
    drawResultCard(0, 0, Paper);

    const Color quiet = ColorLerp(Ink, Paper, 0.20f);
    if (won) {
        type.centered("You are so", 500, 240, 29, Ink, true, 284);
        type.centered("cracked twin", 500, 272, 29, Ink, true, 284);
    } else {
        type.centered(hazard ? "Barrier hit" : "Time's up", 500, 258, 34, Ink, true, 284);
    }
    type.centered(playerName.c_str(), 500, 311, 17, quiet, true, 276);
    type.centered("Time", 500, 345, 15, quiet, true);
    type.centered(formatTime(game.timeTaken()), 500, 367, 46, Ink, true, 284);
    DrawLine(uiX(370), uiY(428), uiX(630), uiY(428), Fade(Ink, 0.25f));

    // A failed save must never be presented as a new record.
    if (bestTimeSaveFailed)
        type.centered("Time not saved", 500, 447, 18, Ink, true, 260);
    else if (won && newBestTime)
        type.centered("New best", 500, 447, 18, Ink, true, 260);
    else if (bestTime >= 0)
        type.centered(TextFormat("Best  %s", formatTime(bestTime)), 500, 447, 18, quiet, true, 260);

    const int size = game.maze().size();
    type.centered(game.mode() == GameMode::ZeroGravity
                      ? TextFormat("%d x %d / Zero gravity", size, size)
                      : TextFormat("%d x %d", size, size),
                  500, 494, 14, quiet, true, 270);
    rlPopMatrix();

    for (int index = 0; index < 2; ++index) {
        const bool active = index == selected;
        const Rectangle button = actionBounds(index);
        DrawRectangleRounded(button, 0.10f, 6, active ? Paper : Panel);
        DrawRectangleRoundedLinesEx(button, 0.10f, 6, std::max(1.0f, uiScale()),
                                    active ? Paper : Border);
        type.centered(index == 0 ? "Again" : "Menu", 395 + index * 210, 587, 20,
                      active ? Ink : Paper, true, 160);
    }
}
