#include "PlayerSelect.h"
#include "Ui.h"
#include "Input.h"

PlayerSelect::PlayerSelect(Players& p) : players(p), selected(0), typing(false), error(0) {}

int PlayerSelect::optionCount() const {
    // all players, plus "+ New player" while there is room
    return players.count() < MAX_PLAYERS ? players.count() + 1 : players.count();
}

Screen PlayerSelect::update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (typing) {
        if (editText(text, MAX_NAME_LENGTH, isNameCharacter)) error = 0;
        if (IsKeyPressed(KEY_ENTER) || menuClicked(entryConfirmBounds())) {
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
        clicked |= menuPointer(uiRect(410, 210 + i * 76, 526, 64), i, selected);
    if (IsKeyPressed(KEY_ESCAPE) || menuClicked(menuBackBounds())) return Screen::MAIN_MENU;

    if (IsKeyPressed(KEY_DELETE) && selected < players.count()) {
        if (players.remove(selected)) {
            error = 0;
            if (selected >= optionCount()) selected = optionCount() - 1;
        }
        else {
            error = 3;
        }
    }

    if (menuConfirmed() || clicked) {
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
    using namespace MenuTheme;
    drawMenuBackdrop(1);
    drawMenuHeading("01 / THE EXPLORERS", "Who is going in?", "Choose a player. Every escape starts with a name.");
    drawMenuBack();
    drawMenuPanel(uiRect(64, 210, 316, 380));
    drawMenuText("EXPLORER REGISTER", 88, 237, 14, Amber);
    // A compass-like maze stamp gives the register its own visual identity.
    Vector2 center = { (float)uiX(222), (float)uiY(365) };
    DrawCircleLinesV(center, (float)uiSize(68), Border);
    DrawCircleLinesV(center, (float)uiSize(53), Fade(Mint, 0.3f));
    DrawLine(uiX(144), uiY(365), uiX(300), uiY(365), Border);
    DrawLine(uiX(222), uiY(287), uiX(222), uiY(443), Border);
    DrawLineEx({ (float)uiX(193), (float)uiY(383) }, { (float)uiX(222), (float)uiY(330) }, 2 * uiScale(), Amber);
    DrawLineEx({ (float)uiX(222), (float)uiY(330) }, { (float)uiX(251), (float)uiY(383) }, 2 * uiScale(), Amber);
    DrawLineEx({ (float)uiX(251), (float)uiY(383) }, { (float)uiX(193), (float)uiY(383) }, 2 * uiScale(), Amber);
    drawMenuText(TextFormat("%02d / %02d", players.count(), MAX_PLAYERS), 88, 466, 38);
    drawMenuText("PLAYER SLOTS FILLED", 88, 514, 14, Muted);
    drawMenuText("Your best times travel with you.", 88, 554, 14, Muted);

    for (int i = 0; i < optionCount(); i++) {
        const bool existing = i < players.count();
        drawMenuButton(uiRect(410, 210 + i * 76, 526, 64),
            existing ? players.name(i).c_str() : "+ New player", i == selected);
        if (existing)
            drawMenuText(TextFormat("%02d", i + 1), 856, 233 + i * 76, 16,
                i == selected ? Amber : Muted);
    }
    drawMenuFooter("UP / DOWN  choose     ENTER  select     DEL  remove player     ESC  back");

    if (typing) {
        drawPopup(640, 330, Amber, Panel);
        drawMenuText("A NEW EXPLORER", 208, 214, 14, Amber);
        drawMenuText("What should we call you?", 208, 244, 30);
        drawMenuPanel(uiRect(208, 292, 584, 62));
        drawTextEntry(text, uiY(304), uiSize(36));
        if (error == 1)
            drawCenteredAt(TextFormat("Use 1-%d letters, digits or _", MAX_NAME_LENGTH),
                uiY(372), uiSize(18), Danger);
        else if (error == 2)
            drawCentered("That name already exists", 372, 18, Danger);
        else if (error == 3)
            drawCentered("Could not save player data", 372, 18, Danger);
        else
            drawCentered("Letters, digits and underscores. Up to 12 characters.", 372, 16, Muted);
        drawEntryActions();
        drawCentered("ENTER  confirm     ESC  cancel", 478, 14, Muted);
    }
    if (!typing && error == 3)
        drawCentered("Could not save player data", 682, 16, Danger);
}
