#include "PlayerSelect.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cctype>

namespace {
Rectangle playerBounds(int index) {
    return uiRect(430, 210 + index * 76, 506, 64);
}

std::string initials(const std::string& name) {
    std::string value = name.substr(0, 2);
    for (char& ch : value)
        ch = (char)std::toupper((unsigned char)ch);
    return value;
}
}

// Retain access to shared profiles and initialize selection, name entry and error display to their
// idle states.
PlayerSelect::PlayerSelect(Players& p) : players(p), selected(0), typing(false), error(0) {
}

// Count selectable profiles and include the new-player option only when profile capacity remains.
int PlayerSelect::optionCount() const {
    // Count saved profiles plus the new-player option when the profile limit has not been reached.
    return players.count() < MAX_PLAYERS ? players.count() + 1 : players.count();
}

// Handle profile selection, name validation and deletion, advancing to level selection only after a
// profile is selected or saved successfully.
Screen PlayerSelect::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (typing) {
        if (editText(text, MAX_NAME_LENGTH, isNameCharacter))
            error = 0;
        if (IsKeyPressed(KEY_ENTER) || menuClicked(entryConfirmBounds())) {
            Players::AddResult r = players.add(text);
            // Select the newly saved profile and continue to level selection only after creation
            // succeeds.
            if (r == Players::ADDED) {
                players.setActive(players.count() - 1);
                typing = false;
                return Screen::LEVEL_SELECT;
            }
            if (r == Players::DUPLICATE)
                error = 2;
            else if (r == Players::SAVE_FAILED)
                error = 3;
            else
                error = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE) || menuClicked(entryCancelBounds())) {
            typing = false;
            error = 0;
        }
        return Screen::PLAYER_SELECT;
    }

    int n = optionCount();
    updateMenuSelection(selected, n);
    bool clicked = false;
    for (int i = 0; i < n; ++i)
        clicked |= menuPointer(playerBounds(i), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds()))
        return Screen::MAIN_MENU;

    if (IsKeyPressed(KEY_DELETE) && selected < players.count()) {
        if (players.remove(selected)) {
            error = 0;
            if (selected >= optionCount())
                selected = optionCount() - 1;
        } else {
            error = 3;
        }
    }

    if (menuConfirmed() || clicked) {
        if (selected < players.count()) {
            players.setActive(selected);
            return Screen::LEVEL_SELECT;
        }
        // Open an empty name-entry dialog when the new-player option is selected.
        typing = true;
        text.clear();
        error = 0;
        // Drain queued character events so the key used to open the dialog is not included in the
        // new name.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::PLAYER_SELECT;
}

// Render a selected-profile preview over the living maze and a compact, readable profile list.
void PlayerSelect::draw() const {
    using namespace MenuTheme;
    const bool existing = selected < players.count();
    const std::string name = existing ? players.name(selected) : "New player";
    const std::string mark = existing ? initials(name) : "+";
    background.draw((float)GetTime(), -0.16f);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), uiY(190), Ink, Fade(Ink, 0));
    drawMenuHeading(type, "01 / PROFILES", "Select player",
                    "Choose a saved player or create a new one.");
    drawMenuBack(type);

    const Vector2 portrait = {(float)uiX(222), (float)uiY(332)};
    DrawPoly(portrait, 6, 94 * uiScale(), 30, Panel);
    DrawPolyLinesEx(portrait, 6, 94 * uiScale(), 30, std::max(1.0f, uiScale()), Border);
    DrawPoly(portrait, 6, 80 * uiScale(), 30, Selected);
    DrawPolyLinesEx(portrait, 6, 80 * uiScale(), 30, std::max(1.5f, 2 * uiScale()), Accent);
    type.centered(mark.c_str(), 222, 282, 68, Paper, true, 128);
    type.centered(existing ? "PLAYER PROFILE" : "CREATE PROFILE", 222, 433, 13, Accent, true);

    drawMenuPanel(uiRect(64, 478, 344, 112));
    DrawRectangle(uiX(64), uiY(496), std::max(2, uiSize(3)), uiSize(50), Accent);
    type.draw(existing ? "SELECTED PLAYER" : "NEW PLAYER", 86, 493, 12, Muted, true);
    type.draw(name.c_str(), 85, 515, 28, Paper, true, 298);
    for (int slot = 0; slot < MAX_PLAYERS; ++slot)
        DrawRectangle(uiX(86 + slot * 20), uiY(564), uiSize(12), uiSize(5),
                      slot < players.count() ? Accent : Border);
    type.draw(TextFormat("%02d / %02d profiles", players.count(), MAX_PLAYERS), 211, 552, 14, Muted,
              false, 173);

    for (int index = 0; index < optionCount(); ++index) {
        const bool saved = index < players.count();
        const bool active = index == selected;
        const int y = 210 + index * 76;
        const Rectangle row = playerBounds(index);
        DrawRectangleRounded(row, 0.08f, 6, active ? Selected : Panel);
        DrawRectangleRoundedLinesEx(row, 0.08f, 6, std::max(1.0f, uiScale()),
                                    active ? Accent : Border);
        if (active)
            DrawRectangle(uiX(430), uiY(y + 16), std::max(2, uiSize(3)), uiSize(32), Accent);
        DrawRectangleRounded(uiRect(446, y + 12, 40, 40), 0.12f, 6, MazeSide);
        const std::string letter = saved ? initials(players.name(index)).substr(0, 1) : "+";
        type.centered(letter.c_str(), 466, y + 16, 24, active ? Accent : Paper, true);
        type.draw(saved ? players.name(index).c_str() : "New player", 504, y + 9, 23,
                  active ? Paper : Muted, true, 334);
        type.draw(saved ? TextFormat("PROFILE %02d", index + 1) : "Create a profile", 505, y + 39,
                  12, active ? Accent : Muted, false, 330);
        const int arrow = 906;
        const Color color = active ? Accent : Border;
        DrawLineEx({(float)uiX(arrow - 5), (float)uiY(y + 26)},
                   {(float)uiX(arrow + 1), (float)uiY(y + 32)}, std::max(1.5f, 2 * uiScale()),
                   color);
        DrawLineEx({(float)uiX(arrow + 1), (float)uiY(y + 32)},
                   {(float)uiX(arrow - 5), (float)uiY(y + 38)}, std::max(1.5f, 2 * uiScale()),
                   color);
    }
    drawMenuFooter(type,
                   "UP / DOWN  choose     ENTER  select     DEL  remove player     ESC  back");

    if (typing) {
        drawPopup(640, 330, Accent, Panel, 0.88f);
        type.draw("NEW PLAYER", 208, 210, 13, Accent, true);
        type.draw("Player name", 208, 238, 32, Paper, true);
        drawMenuPanel(uiRect(208, 292, 584, 62));
        const std::string entry = text + ((((int)(GetTime() * 2)) % 2 == 0) ? "|" : "");
        type.centered(entry.c_str(), 500, 301, 34, Paper, true, 544);
        if (error == 1)
            type.centered(TextFormat("Use 1-%d letters, digits or _", MAX_NAME_LENGTH), 500, 371,
                          17, Danger, false, 584);
        else if (error == 2)
            type.centered("That name already exists", 500, 371, 17, Danger);
        else if (error == 3)
            type.centered("Could not save player data", 500, 371, 17, Danger);
        else
            type.centered("Letters, digits and underscores. Up to 12 characters.", 500, 371, 16,
                          Muted, false, 584);
        DrawRectangleRounded(entryConfirmBounds(), 0.1f, 6, Selected);
        DrawRectangleRoundedLinesEx(entryConfirmBounds(), 0.1f, 6, std::max(1.0f, uiScale()),
                                    Accent);
        DrawRectangleRounded(entryCancelBounds(), 0.1f, 6, Panel);
        DrawRectangleRoundedLinesEx(entryCancelBounds(), 0.1f, 6, std::max(1.0f, uiScale()),
                                    Border);
        type.centered("Confirm", 405, 415, 19, Paper, true);
        type.centered("Cancel", 595, 415, 19, Muted, true);
        type.centered("ENTER  confirm     ESC  cancel", 500, 476, 14, Muted);
    }
    if (!typing && error == 3)
        type.centered("Could not save player data", 500, 678, 15, Danger);
}
