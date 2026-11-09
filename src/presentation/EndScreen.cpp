#include "EndScreen.h"
#include "Input.h"
#include "Ui.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

namespace {
// Return the scaled rectangle for one result-screen action using fixed reference spacing.
// Keyboard selection and pointer hit testing share the same action ordering.
Rectangle actionBounds(int index) {
    return uiRect(302 + index * 210, 576, 186, 48);
}

// Format nonnegative elapsed seconds as minutes, seconds, and tenths using raylib's temporary
// TextFormat buffer. Consume the returned text promptly rather than retaining it across later
// formatting calls.
const char* formatTime(float value) {
    const int tenths = (int)(std::max(0.0f, value) * 10);
    return TextFormat("%02d:%02d.%d", tenths / 600, tenths / 10 % 60, tenths % 10);
}

// Draw the main rounded result card at a reference-layout position plus the supplied pixel
// offsets. The same shape is reused for the card and its displaced shadow.
void drawResultCard(float offsetX, float offsetY, Color color) {
    Rectangle body = uiRect(330, 224, 340, 312);
    body.x += offsetX;
    body.y += offsetY;
    DrawRectangleRounded(body, 0.06f, 8, color);
}
} // namespace

// Restart the result-card entrance animation and select its first action. Application calls this
// once when a run finishes so a previous result's animation and selection do not carry over.
void EndScreen::reset() {
    animationTime = 0;
    selected = 0;
}

// Advance result-screen animation and handle keyboard or pointer actions for retry and
// navigation. Return the requested Screen while the finished Game itself remains frozen.
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

// Render the frozen run's outcome, distinguishing victory, timeout, and obstacle contact. Show
// the player's time, best-time status, save failures, and selected navigation action without
// attempting persistence.
void EndScreen::draw(const Game& game, const std::string& playerName, float bestTime,
                     bool newBestTime, bool bestTimeSaveFailed) const {
    using namespace MenuTheme;
    const bool won = game.won();
    const bool hazard = game.failureReason() == FailureReason::ObstacleHit;
    const Color card = ColorLerp(Ink, BLACK, .45f);
    const Color surface = ColorLerp(Ink, Panel, .12f);
    const Color focus = ColorLerp(surface, Accent, .12f);
    const Color edge = ColorLerp(Ink, Accent, .23f);
    const Color brass = ColorLerp(Accent, Panel, .30f);
    const Color quiet = ColorLerp(Paper, Ink, .30f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, .78f));

    const float progress = std::clamp(animationTime / 0.4f, 0.0f, 1.0f);
    const float lift = -12 * (1 - progress) * (1 - progress) * uiScale();
    rlPushMatrix();
    rlTranslatef(0, lift, 0);
    drawResultCard(5 * uiScale(), 7 * uiScale(), Fade(BLACK, .40f));
    drawResultCard(0, 0, card);
    DrawRectangleRoundedLinesEx(uiRect(330, 224, 340, 312), .06f, 8, std::max(1.0f, uiScale()),
                                edge);

    if (won) {
        type.centered("You are so", 500, 240, 29, Paper, true, 284);
        type.centered("cracked twin", 500, 272, 29, Paper, true, 284);
    } else {
        type.centered(hazard ? "Barrier hit" : "Time's up", 500, 258, 34, Paper, true, 284);
    }
    type.centered(playerName.c_str(), 500, 311, 17, quiet, true, 276);
    type.centered("Time", 500, 345, 15, quiet, true);
    type.centered(formatTime(game.timeTaken()), 500, 367, 46, Paper, true, 284);
    DrawLine(uiX(370), uiY(428), uiX(630), uiY(428), edge);

    // A failed save must never be presented as a new record.
    if (bestTimeSaveFailed)
        type.centered("Time not saved", 500, 447, 18, Danger, true, 260);
    else if (won && newBestTime)
        type.centered("New best", 500, 447, 18, brass, true, 260);
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
        DrawRectangleRounded(button, 0.10f, 6, active ? focus : surface);
        DrawRectangleRoundedLinesEx(button, 0.10f, 6, std::max(1.0f, uiScale()),
                                    active ? brass : edge);
        if (active)
            DrawRectangle((int)button.x, (int)button.y + uiSize(12), std::max(2, uiSize(2)),
                          uiSize(24), brass);
        type.centered(index == 0 ? "Again" : "Menu", 395 + index * 210, 587, 20,
                      active ? Paper : quiet, true, 160);
    }
}
