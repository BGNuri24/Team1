#include "PlayerSelect.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cctype>

namespace {
// Return the reference-scaled rectangle for one profile-list row. Drawing and pointer selection
// use identical row spacing so the visible entry matches its interactive region.
Rectangle playerBounds(int index) {
    return uiRect(280, 232 + index * 58, 448, 52);
}

// Return the smaller delete-button rectangle inside a profile row. Keeping this separate from
// playerBounds lets update distinguish profile selection from a deletion request.
Rectangle playerDeleteBounds(int index) {
    return uiRect(672, 246 + index * 58, 28, 28);
}

// Draw a profile-screen line using shared coordinate scaling and a minimum-one-pixel thickness.
// Cards and delete icons use the same helper to remain consistent when resized.
void profileRule(int x1, int y1, int x2, int y2, Color color, float width = 1) {
    DrawLineEx({(float)uiX(x1), (float)uiY(y1)}, {(float)uiX(x2), (float)uiY(y2)},
               std::max(1.0f, width * uiScale()), color);
}

// Draw the small bin icon for a profile row from scaled lines and a rectangle. The index
// determines its vertical placement; hit testing uses playerDeleteBounds separately.
void profileDeleteIcon(int index, Color color) {
    const int y = 232 + index * 58;
    profileRule(680, y + 19, 692, y + 19, color);
    profileRule(683, y + 16, 689, y + 16, color);
    DrawRectangleLinesEx(uiRect(682, y + 22, 8, 10), std::max(1.0f, uiScale()), color);
}

// Take at most the first two bytes of a validated ASCII profile name and uppercase them for its
// badge. Cast to unsigned char before toupper to avoid signed-character undefined behavior.
std::string initials(const std::string& name) {
    std::string value = name.substr(0, 2);
    for (char& ch : value)
        ch = (char)std::toupper((unsigned char)ch);
    return value;
}
} // namespace

// Borrow the application's Players object and initialize selection, name-entry, and feedback
// state. The reference does not own profiles; additions and removals go through the shared logic
// layer.
PlayerSelect::PlayerSelect(Players& p) : players(p), selected(0), typing(false), error(0) {}

// Count the currently selectable profile entries and available add-profile action. Keyboard
// navigation uses this value to stay within the list as profiles are added or removed.
int PlayerSelect::optionCount() const {
    // Count saved profiles plus the new-player option when the profile limit has not been reached.
    return players.count() < MAX_PLAYERS ? players.count() + 1 : players.count();
}

// Handle profile selection, validated name entry, and deletion while keeping the active profile
// in shared Players state. Translate addition failures into feedback and return navigation only
// after an appropriate confirmation.
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
            background.detail(false);
        }
        return Screen::PLAYER_SELECT;
    }

    int n = optionCount();
    updateMenuSelection(selected, n);
    const int deleting =
        selected < players.count() && menuClicked(playerDeleteBounds(selected)) ? selected : -1;
    bool clicked = false;
    for (int i = 0; i < n; ++i)
        clicked |= menuPointer(playerBounds(i), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds()))
        return Screen::MAIN_MENU;

    if (deleting >= 0 || (IsKeyPressed(KEY_DELETE) && selected < players.count())) {
        if (players.remove(deleting >= 0 ? deleting : selected)) {
            error = 0;
            if (selected >= optionCount())
                selected = optionCount() - 1;
        } else {
            error = 3;
        }
        return Screen::PLAYER_SELECT;
    }

    if (menuConfirmed() || clicked) {
        if (selected < players.count()) {
            players.setActive(selected);
            return Screen::LEVEL_SELECT;
        }
        // Open an empty name-entry dialog when the new-player option is selected.
        typing = true;
        background.detail(true);
        text.clear();
        error = 0;
        // Drain queued character events so the key used to open the dialog is not included in the
        // new name.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::PLAYER_SELECT;
}

// Render saved profiles, their active and selected states, delete actions, and the name-entry
// dialog. Display feedback supplied by update without performing database operations from
// drawing.
void PlayerSelect::draw() const {
    using namespace MenuTheme;
    background.draw((float)GetTime());
    drawMenuBack(type);
    const Color page = ColorLerp(Paper, Accent, .12f);
    const Color rule = Fade(Ink, .18f);
    DrawRectangleRec(uiRect(244, 168, 526, 426), Fade(Ink, .65f));
    DrawRectangleRec(uiRect(234, 156, 32, 420), Panel);
    DrawRectangleRec(uiRect(254, 156, 508, 420), page);
    DrawRectangleLinesEx(uiRect(254, 156, 508, 420), std::max(1.0f, uiScale()),
                         ColorLerp(Accent, Ink, .4f));
    profileRule(248, 162, 248, 570, Accent);
    profileRule(270, 164, 270, 568, Fade(Ink, .15f));
    for (int y : {196, 274, 352, 430, 508})
        profileRule(238, y, 257, y, Border, 3);
    DrawRectangleRec(uiRect(742, 156, 20, 32), ColorLerp(page, Accent, .45f));
    type.draw("Players", 286, 178, 32, Ink, true);
    type.draw(TextFormat("%d / %d", players.count(), MAX_PLAYERS), 676, 193, 14, Fade(Ink, .6f),
              false, 66);
    profileRule(286, 222, 728, 222, Fade(Ink, .36f));

    for (int index = 0; index < MAX_PLAYERS; ++index) {
        const int y = 232 + index * 58;
        profileRule(286, y + 52, 728, y + 52, rule);
        if (index >= optionCount())
            continue;
        const bool saved = index < players.count();
        const bool active = index == selected;
        if (active) {
            DrawRectangleRec(playerBounds(index), ColorLerp(page, Accent, .32f));
            DrawRectangleRec(uiRect(280, y + 10, 3, 32), Ink);
        }
        const Color mark = active ? Ink : Fade(Ink, .65f);
        DrawCircleV({(float)uiX(304), (float)uiY(y + 26)}, 17 * uiScale(), mark);
        const std::string letters = saved ? initials(players.name(index)) : "+";
        type.centeredIn(letters.c_str(), uiRect(286, y + 8, 36, 36), 18, page);
        type.draw(saved ? players.name(index).c_str() : "New player", 338, y + 12, 23, Ink, active,
                  310);
        if (active) {
            if (saved) {
                const bool over =
                    CheckCollisionPointRec(GetMousePosition(), playerDeleteBounds(index));
                profileDeleteIcon(index, over ? Ink : Fade(Ink, .5f));
            }
            profileRule(710, y + 21, 716, y + 26, Ink);
            profileRule(716, y + 26, 710, y + 31, Ink);
        }
    }
    profileRule(286, 548, 728, 548, rule);

    if (typing) {
        drawPopup(640, 330, Accent, page, .88f);
        type.draw("New player", 208, 226, 32, Ink, true);
        type.draw("Name", 208, 273, 13, Fade(Ink, .65f));
        type.draw(TextFormat("%d / %d", (int)text.size(), MAX_NAME_LENGTH), 724, 273, 13,
                  Fade(Ink, .65f), false, 68);
        DrawRectangleRec(uiRect(208, 296, 584, 58), Fade(Ink, .04f));
        profileRule(208, 354, 792, 354, Fade(Ink, .45f));
        const std::string entry = text + ((((int)(GetTime() * 2)) % 2 == 0) ? "|" : "");
        type.centered(entry.c_str(), 500, 300, 34, Ink, true, 544);
        if (error == 1)
            type.centered(TextFormat("Use 1-%d letters, numbers or _", MAX_NAME_LENGTH), 500, 373,
                          16, Ink, false, 584);
        else if (error == 2)
            type.centered("Name already taken", 500, 373, 16, Ink);
        else if (error == 3)
            type.centered("Could not save player", 500, 373, 16, Ink);
        DrawRectangleRec(entryConfirmBounds(), Ink);
        DrawRectangleLinesEx(entryCancelBounds(), std::max(1.0f, uiScale()), Fade(Ink, .45f));
        type.centered("Add", 405, 415, 19, page, true);
        type.centered("Cancel", 595, 415, 19, Ink);
    }
    if (!typing && error == 3)
        type.centered("Could not remove player", 500, 602, 16, Paper);
}
