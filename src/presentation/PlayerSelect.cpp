#include "PlayerSelect.h"
#include "Ui.h"
#include "Input.h"

PlayerSelect::PlayerSelect(Players& p) : players(p), selected(0), typing(false), error(0) {}

int PlayerSelect::optionCount() const {
    // all players, plus "+ New player" while there is room
    return players.count() < MAX_PLAYERS ? players.count() + 1 : players.count();
}

Screen PlayerSelect::update() {
    if (typing) {
        if (editText(text, MAX_NAME_LENGTH, isNameCharacter)) error = 0;
        if (IsKeyPressed(KEY_ENTER)) {
            Players::AddResult r = players.add(text);
            // the new player plays
            if (r == Players::ADDED) {
                players.setActive(players.count() - 1);   
                typing = false;
                return Screen::LEVEL_SELECT;
            }
            if (r == Players::DUPLICATE) error = 2;
            else if (r == Players::SAVE_FAILED) error = 3;
            else error = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            typing = false;
            error = 0;
        }
        return Screen::PLAYER_SELECT;
    }

    int n = optionCount();
    updateMenuSelection(selected, n);
    if (IsKeyPressed(KEY_ESCAPE)) return Screen::MAIN_MENU;

    if (IsKeyPressed(KEY_DELETE) && selected < players.count()) {
        if (players.remove(selected)) {
            error = 0;
            if (selected >= optionCount()) selected = optionCount() - 1;
        }
        else {
            error = 3;
        }
    }

    if (menuConfirmed()) {
        if (selected < players.count()) {
            players.setActive(selected);
            return Screen::LEVEL_SELECT;
        }
        // "+ New player"
        typing = true;                              
        text.clear();
        error = 0;
        // drop leftover typed characters
        while (GetCharPressed() > 0) {}             
    }
    return Screen::PLAYER_SELECT;
}

void PlayerSelect::draw() const {
    ClearBackground(Color{ 20, 22, 30, 255 });
    drawCentered("SELECT PLAYER", 80, 70, GOLD);

    int n = optionCount();
    for (int i = 0; i < n; i++) {
        const char* mark = (i == selected) ? "> " : "  ";
        const char* line = (i < players.count())
            ? TextFormat("%s%s", mark, players.name(i).c_str())
            : TextFormat("%s+ New player", mark);
        drawCentered(line, 200 + i * 65, 40, i == selected ? GOLD : GRAY);
    }
    drawCentered("UP/DOWN choose    ENTER select    DEL remove    ESC back", 630, 22, LIGHTGRAY);

    if (typing) {
        int y = (int)drawPopup(640, 260).y;

        drawCenteredAt("New player name", y + uiSize(30), uiSize(36), WHITE);

        drawTextEntry(text, y + uiSize(100), uiSize(50));

        if (error == 1)
            drawCenteredAt(TextFormat("Use 1-%d letters, digits or _", MAX_NAME_LENGTH),
                y + uiSize(190), uiSize(26), RED);
        else if (error == 2)
            drawCenteredAt("That name already exists", y + uiSize(190), uiSize(26), RED);
        else if (error == 3)
            drawCenteredAt("Could not save player data", y + uiSize(190), uiSize(26), RED);
        else
            drawCenteredAt("ENTER confirm    ESC cancel", y + uiSize(200), uiSize(24), GRAY);
    }
    if (!typing && error == 3)
        drawCentered("Could not save player data", 670, 22, RED);
}
