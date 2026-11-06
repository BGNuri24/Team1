#include "EndScreen.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cmath>

namespace {
Rectangle actionBounds(int index) {
    return uiRect(374 + index * 270, 578, 252, 52);
}

// Keep the report's times independent of the score reveal animation.
const char* formatTime(float value) {
    const int tenths = (int)(std::max(0.0f, value) * 10);
    return TextFormat("%02d:%02d.%d", tenths / 600, tenths / 10 % 60, tenths % 10);
}

// Draw a trophy for a win and an hourglass or crossed barrier for a failed run.
void resultMark(bool won, bool hazard, Color accent, float reveal) {
    const Vector2 center = {(float)uiX(230), (float)uiY(282)};
    const float radius = (74 + (1 - reveal) * 10) * uiScale();
    DrawPoly(center, 6, radius, 30, MenuTheme::Panel);
    DrawPolyLinesEx(center, 6, radius, 30, std::max(1.0f, 2 * uiScale()), accent);
    auto line = [&](int x1, int y1, int x2, int y2) {
        DrawLineEx({(float)uiX(x1), (float)uiY(y1)}, {(float)uiX(x2), (float)uiY(y2)},
                   std::max(1.0f, 3 * uiScale()), accent);
    };
    if (won) {
        line(208, 253, 252, 253);
        line(208, 253, 214, 285);
        line(252, 253, 246, 285);
        line(214, 285, 230, 297);
        line(246, 285, 230, 297);
        line(230, 297, 230, 317);
        line(214, 319, 246, 319);
        line(208, 259, 196, 259);
        line(196, 259, 199, 280);
        line(199, 280, 213, 286);
        line(252, 259, 264, 259);
        line(264, 259, 261, 280);
        line(261, 280, 247, 286);
    } else if (hazard) {
        line(209, 255, 251, 309);
        line(251, 255, 209, 309);
        line(205, 245, 255, 245);
        line(205, 319, 255, 319);
    } else {
        line(208, 249, 252, 249);
        line(208, 315, 252, 315);
        line(212, 249, 212, 262);
        line(248, 249, 248, 262);
        line(212, 262, 248, 302);
        line(248, 262, 212, 302);
        line(212, 302, 212, 315);
        line(248, 302, 248, 315);
    }
}
}

void EndScreen::reset() {
    animationTime = 0;
    selected = 0;
}

// Offer an immediate new run with the same settings, or return to the main menu.
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

// Present the actual run data with a short score reveal and a separate saved-record status.
void EndScreen::draw(const Game& game, const std::string& playerName, float bestTime,
                     bool newBestTime, bool bestTimeSaveFailed) const {
    using namespace MenuTheme;
    const bool won = game.won();
    const bool hazard = game.failureReason() == FailureReason::ObstacleHit;
    const Color result = won ? Secondary : Danger;
    const float progress = std::clamp(animationTime / 0.8f, 0.0f, 1.0f);
    const float reveal = 1 - std::pow(1 - progress, 3.0f);
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(Ink, 0.96f));
    DrawRectangle(uiX(104), uiY(82), uiSize(28), std::max(2, uiSize(3)), result);
    type.draw("T1 / RUN RESULTS", 146, 74, 14, Muted, true);
    DrawLine(uiX(104), uiY(110), uiX(896), uiY(110), Border);

    // A diagram of this run's maze sits behind the result badge.
    const int cell = std::max(1, uiSize(224) / game.maze().size());
    const int board = cell * game.maze().size();
    const int originX = uiX(230) - board / 2, originY = uiY(282) - board / 2;
    for (int row = 0; row < game.maze().size(); ++row)
        for (int col = 0; col < game.maze().size(); ++col)
            if (game.maze().cell(row, col) == '#')
                DrawRectangle(originX + col * cell, originY + row * cell, std::max(1, cell - 1),
                              std::max(1, cell - 1), Fade(MazeTop, 0.3f));
    resultMark(won, hazard, result, reveal);
    type.centered(TextFormat("%d x %d", game.maze().size(), game.maze().size()), 230, 369, 22,
                  Paper, true, 230);
    type.centered(game.mode() == GameMode::ZeroGravity ? "ZERO GRAVITY" : "NORMAL GRAVITY", 230,
                  400, 12, Muted, true, 230);

    type.draw(won      ? "MAZE COMPLETE"
              : hazard ? "HAZARD COLLISION"
                       : "TIME EXPIRED",
              390, 140, 14, result, true, 506);
    type.draw(won ? "ESCAPED" : "RUN ENDED", 387, 167, 54, Paper, true, 509);
    type.draw(playerName.c_str(), 392, 240, 22, Accent, true, 500);
    type.draw("TOTAL POINTS", 392, 302, 13, Muted, true);
    type.draw(TextFormat("%d", (int)std::round(game.score() * reveal)), 387, 321, 74, Paper, true,
              506);
    DrawRectangle(uiX(392), uiY(411), uiSize(504), std::max(2, uiSize(3)), Border);
    DrawRectangle(uiX(392), uiY(411), (int)(uiSize(504) * reveal), std::max(2, uiSize(3)), result);

    const int columns[] = {104, 374, 644};
    for (int index = 0; index < 3; ++index)
        drawMenuPanel(uiRect(columns[index], 440, 252, 82));
    type.draw(won ? "ESCAPE TIME" : "TIME IN MAZE", 122, 451, 12, Muted, true);
    type.draw(formatTime(game.timeTaken()), 121, 473, 29, Paper, true, 218);
    type.draw("TIME LEFT", 392, 451, 12, Muted, true);
    type.draw(formatTime(game.timeLeft()), 391, 473, 29, Paper, true, 218);
    type.draw(bestTimeSaveFailed ? "RECORD NOT SAVED"
              : newBestTime      ? "NEW PERSONAL BEST"
                                 : "PERSONAL BEST",
              662, 451, 12,
              bestTimeSaveFailed ? Danger
              : newBestTime      ? Accent
                                 : Muted,
              true, 218);
    type.draw(bestTimeSaveFailed ? "UNSAVED"
              : bestTime < 0     ? "--:--.-"
                                 : formatTime(bestTime),
              661, 473, 29,
              bestTimeSaveFailed ? Danger
              : newBestTime      ? Accent
                                 : Paper,
              true, 218);

    if (bestTimeSaveFailed)
        type.draw("Could not save this result.", 104, 537, 16, Danger, false, 792);
    else if (newBestTime)
        type.draw("New personal best", 104, 537, 16, Accent, true, 792);
    else if (!won)
        type.draw(hazard ? "Flip gravity to clear hazard barriers." : "The time limit was reached.",
                  104, 537, 16, Muted, false, 792);

    type.draw("LEFT / RIGHT  choose", 104, 595, 13, Muted, false, 240);
    for (int index = 0; index < 2; ++index) {
        const bool active = index == selected;
        const Rectangle button = actionBounds(index);
        DrawRectangleRounded(button, 0.08f, 6, active ? Selected : Panel);
        DrawRectangleRoundedLinesEx(button, 0.08f, 6, std::max(1.0f, uiScale()),
                                    active ? Accent : Border);
        if (active)
            DrawRectangle((int)button.x, (int)button.y + uiSize(14), std::max(2, uiSize(3)),
                          uiSize(24), Accent);
        type.centered(index == 0 ? (won ? "Play again" : "Try again") : "Main menu",
                      500 + index * 270, 590, 20, active ? Paper : Muted, true, 220);
    }
    type.centered("ENTER  confirm     ESC  main menu", 500, 652, 14, Muted, false, 792);
}
