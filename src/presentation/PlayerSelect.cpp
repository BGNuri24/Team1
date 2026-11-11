#include "PlayerSelect.h"
#include "Input.h"
#include "Ui.h"
#include <algorithm>
#include <cctype>

namespace {
// Return the reference-scaled rectangle for one profile-list row. Drawing and pointer selection
// use identical row spacing so the visible entry matches its interactive region.
Rectangle PlayerBounds(int Index) {
    return UiRect(280, 232 + Index * 58, 448, 52);
}

// Return the smaller delete-button rectangle inside a profile row. Keeping this separate from
// playerBounds lets update distinguish profile selection from a deletion request.
Rectangle PlayerDeleteBounds(int Index) {
    return UiRect(672, 246 + Index * 58, 28, 28);
}

// Draw a profile-screen line using shared coordinate scaling and a minimum-one-pixel thickness.
// Cards and delete icons use the same helper to remain consistent when resized.
void ProfileRule(int X1, int Y1, int X2, int Y2, Color CurrentColor, float Width = 1) {
    DrawLineEx({(float)UiX(X1), (float)UiY(Y1)}, {(float)UiX(X2), (float)UiY(Y2)},
               std::max(1.0f, Width * UiScale()), CurrentColor);
}

// Draw the small bin icon for a profile row from scaled lines and a rectangle. The index
// determines its vertical placement; hit testing uses playerDeleteBounds separately.
void ProfileDeleteIcon(int Index, Color CurrentColor) {
    const int CurrentY = 232 + Index * 58;
    ProfileRule(680, CurrentY + 19, 692, CurrentY + 19, CurrentColor);
    ProfileRule(683, CurrentY + 16, 689, CurrentY + 16, CurrentColor);
    DrawRectangleLinesEx(UiRect(682, CurrentY + 22, 8, 10), std::max(1.0f, UiScale()), CurrentColor);
}

// Take at most the first two bytes of a validated ASCII profile name and uppercase them for its
// badge. Cast to unsigned char before toupper to avoid signed-character undefined behavior.
std::string Initials(const std::string& CurrentName) {
    std::string Value = CurrentName.substr(0, 2);
    for (char& Ch : Value)
        Ch = (char)std::toupper((unsigned char)Ch);
    return Value;
}
} // namespace

// Borrow the application's Players object and initialize selection, name-entry, and feedback
// state. The reference does not own profiles; additions and removals go through the shared logic
// layer.
PlayerSelect::PlayerSelect(Players& P) : CurrentPlayers(P), CurrentSelected(0), Typing(false), Error(0) {}

// Count the currently selectable profile entries and available add-profile action. Keyboard
// navigation uses this value to stay within the list as profiles are added or removed.
int PlayerSelect::OptionCount() const {
    // Count saved profiles plus the new-player option when the profile limit has not been reached.
    return CurrentPlayers.Count() < MaxPlayers ? CurrentPlayers.Count() + 1 : CurrentPlayers.Count();
}

// Handle profile selection, validated name entry, and deletion while keeping the active profile
// in shared Players state. Translate addition failures into feedback and return navigation only
// after an appropriate confirmation.
Screen PlayerSelect::Update() {
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    if (Typing) {
        if (EditText(CurrentText, MaxNameLength, IsNameCharacter))
            Error = 0;
        if (IsKeyPressed(KEY_ENTER) || MenuClicked(EntryConfirmBounds())) {
            Players::AddResult CurrentR = CurrentPlayers.Add(CurrentText);
            // Select the newly saved profile and continue to level selection only after creation
            // succeeds.
            if (CurrentR == Players::ADDED) {
                CurrentPlayers.SetActive(CurrentPlayers.Count() - 1);
                Typing = false;
                return Screen::LevelSelect;
            }
            if (CurrentR == Players::DUPLICATE)
                Error = 2;
            else if (CurrentR == Players::SaveFailed)
                Error = 3;
            else
                Error = 1;
        }
        if (IsKeyPressed(KEY_ESCAPE) || MenuClicked(EntryCancelBounds())) {
            Typing = false;
            Error = 0;
            Background.Detail(false);
        }
        return Screen::PlayerSelect;
    }

    int N = OptionCount();
    UpdateMenuSelection(CurrentSelected, N);
    const int CurrentDeleting =
        CurrentSelected < CurrentPlayers.Count() && MenuClicked(PlayerDeleteBounds(CurrentSelected)) ? CurrentSelected : -1;
    bool Clicked = false;
    for (int I = 0; I < N; ++I)
        Clicked |= MenuPointer(PlayerBounds(I), I, CurrentSelected);
    if (IsKeyPressed(KEY_ESCAPE) || MenuClicked(MenuBackBounds()))
        return Screen::MainMenu;

    if (CurrentDeleting >= 0 || (IsKeyPressed(KEY_DELETE) && CurrentSelected < CurrentPlayers.Count())) {
        if (CurrentPlayers.Remove(CurrentDeleting >= 0 ? CurrentDeleting : CurrentSelected)) {
            Error = 0;
            if (CurrentSelected >= OptionCount())
                CurrentSelected = OptionCount() - 1;
        } else {
            Error = 3;
        }
        return Screen::PlayerSelect;
    }

    if (MenuConfirmed() || Clicked) {
        if (CurrentSelected < CurrentPlayers.Count()) {
            CurrentPlayers.SetActive(CurrentSelected);
            return Screen::LevelSelect;
        }
        // Open an empty name-entry dialog when the new-player option is selected.
        Typing = true;
        Background.Detail(true);
        CurrentText.clear();
        Error = 0;
        // Drain queued character events so the key used to open the dialog is not included in the
        // new name.
        while (GetCharPressed() > 0) {
        }
    }
    return Screen::PlayerSelect;
}

// Render saved profiles, their active and selected states, delete actions, and the name-entry
// dialog. Display feedback supplied by update without performing database operations from
// drawing.
void PlayerSelect::Draw() const {
    using namespace MenuTheme;
    Background.Draw((float)GetTime());
    DrawMenuBack(Type);
    const Color Page = ColorLerp(Paper, Accent, .12f);
    const Color Rule = Fade(Ink, .18f);
    DrawRectangleRec(UiRect(244, 168, 526, 426), Fade(Ink, .65f));
    DrawRectangleRec(UiRect(234, 156, 32, 420), Panel);
    DrawRectangleRec(UiRect(254, 156, 508, 420), Page);
    DrawRectangleLinesEx(UiRect(254, 156, 508, 420), std::max(1.0f, UiScale()),
                         ColorLerp(Accent, Ink, .4f));
    ProfileRule(248, 162, 248, 570, Accent);
    ProfileRule(270, 164, 270, 568, Fade(Ink, .15f));
    for (int CurrentY : {196, 274, 352, 430, 508})
        ProfileRule(238, CurrentY, 257, CurrentY, Border, 3);
    DrawRectangleRec(UiRect(742, 156, 20, 32), ColorLerp(Page, Accent, .45f));
    Type.Draw("Players", 286, 178, 32, Ink, true);
    Type.Draw(TextFormat("%d / %d", CurrentPlayers.Count(), MaxPlayers), 676, 193, 14, Fade(Ink, .6f),
              false, 66);
    ProfileRule(286, 222, 728, 222, Fade(Ink, .36f));

    for (int Index = 0; Index < MaxPlayers; ++Index) {
        const int CurrentY = 232 + Index * 58;
        ProfileRule(286, CurrentY + 52, 728, CurrentY + 52, Rule);
        if (Index >= OptionCount())
            continue;
        const bool Saved = Index < CurrentPlayers.Count();
        const bool Active = Index == CurrentSelected;
        if (Active) {
            DrawRectangleRec(PlayerBounds(Index), ColorLerp(Page, Accent, .32f));
            DrawRectangleRec(UiRect(280, CurrentY + 10, 3, 32), Ink);
        }
        const Color CurrentMark = Active ? Ink : Fade(Ink, .65f);
        DrawCircleV({(float)UiX(304), (float)UiY(CurrentY + 26)}, 17 * UiScale(), CurrentMark);
        const std::string Letters = Saved ? Initials(CurrentPlayers.Name(Index)) : "+";
        Type.CenteredIn(Letters.c_str(), UiRect(286, CurrentY + 8, 36, 36), 18, Page);
        Type.Draw(Saved ? CurrentPlayers.Name(Index).c_str() : "New player", 338, CurrentY + 12, 23, Ink, Active,
                  310);
        if (Active) {
            if (Saved) {
                const bool Over =
                    CheckCollisionPointRec(GetMousePosition(), PlayerDeleteBounds(Index));
                ProfileDeleteIcon(Index, Over ? Ink : Fade(Ink, .5f));
            }
            ProfileRule(710, CurrentY + 21, 716, CurrentY + 26, Ink);
            ProfileRule(716, CurrentY + 26, 710, CurrentY + 31, Ink);
        }
    }
    ProfileRule(286, 548, 728, 548, Rule);

    if (Typing) {
        DrawPopup(640, 330, Accent, Page, .88f);
        Type.Draw("New player", 208, 226, 32, Ink, true);
        Type.Draw("Name", 208, 273, 13, Fade(Ink, .65f));
        Type.Draw(TextFormat("%d / %d", (int)CurrentText.size(), MaxNameLength), 724, 273, 13,
                  Fade(Ink, .65f), false, 68);
        DrawRectangleRec(UiRect(208, 296, 584, 58), Fade(Ink, .04f));
        ProfileRule(208, 354, 792, 354, Fade(Ink, .45f));
        const std::string Entry = CurrentText + ((((int)(GetTime() * 2)) % 2 == 0) ? "|" : "");
        Type.Centered(Entry.c_str(), 500, 300, 34, Ink, true, 544);
        if (Error == 1)
            Type.Centered(TextFormat("Use 1-%d letters, numbers or _", MaxNameLength), 500, 373,
                          16, Ink, false, 584);
        else if (Error == 2)
            Type.Centered("Name already taken", 500, 373, 16, Ink);
        else if (Error == 3)
            Type.Centered("Could not save player", 500, 373, 16, Ink);
        DrawRectangleRec(EntryConfirmBounds(), Ink);
        DrawRectangleLinesEx(EntryCancelBounds(), std::max(1.0f, UiScale()), Fade(Ink, .45f));
        Type.Centered("Add", 405, 415, 19, Page, true);
        Type.Centered("Cancel", 595, 415, 19, Ink);
    }
    if (!Typing && Error == 3)
        Type.Centered("Could not remove player", 500, 602, 16, Paper);
}
