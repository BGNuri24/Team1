#include "../presentation/Input.h"
#include "../presentation/MainMenu.h"
#include "../presentation/Ui.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Drive menu input without sending keys or moving the desktop pointer.
int MenuSelection = -1;
bool MenuConfirm = false, MenuClick = false;
Vector2 MenuPosition{-100, -100}, MenuMovement{};
// Supply a fixed animation time for main-menu rendering under test. Stable timing keeps
// screenshots and hover checks independent of real wall-clock time.
double MenuTime() {
    return 4;
}
// Return the fixture's synthetic pointer position for main-menu hit testing. Tests place it at
// option centers without moving the user's physical cursor.
Vector2 MenuMousePosition() {
    return MenuPosition;
}
// Return the synthetic pointer movement used to distinguish active hovering from a stationary
// cursor. This verifies that keyboard navigation keeps selection when the mouse has not moved.
Vector2 MenuMouseDelta() {
    return MenuMovement;
}
// Return the fixture's click flag instead of reading a mouse button. The menu's left-click path
// is exercised without sending input to a real window.
bool MenuMousePressed(int) {
    return MenuClick;
}
// Return the fixture's confirmation event for menu actions. Tests can trigger keyboard
// confirmation independently of pointer clicks.
bool MenuConfirmation() {
    return MenuConfirm;
}
// Apply a requested synthetic option index only when the fixture provides one, clamping it to the
// current option range. This substitutes deterministic navigation for real keyboard events.
void MenuChoose(int& CurrentSelected, int CurrentCount) {
    if (MenuSelection >= 0)
        CurrentSelected = std::clamp(MenuSelection, 0, CurrentCount - 1);
}
}

#define GetTime MenuTime
#define GetMousePosition MenuMousePosition
#define GetMouseDelta MenuMouseDelta
#define IsMouseButtonPressed MenuMousePressed
#define MenuConfirmed MenuConfirmation
#define UpdateMenuSelection MenuChoose
#include "../presentation/MainMenu.cpp"
#undef GetTime
#undef GetMousePosition
#undef GetMouseDelta
#undef IsMouseButtonPressed
#undef MenuConfirmed
#undef UpdateMenuSelection

namespace {
// Throw a descriptive exception when a menu or journey expectation fails. The UI test executable
// catches it and reports a failing process status.
void ExpectMenu(bool Condition, const char* Message) {
    if (!Condition)
        throw std::runtime_error(Message);
}

// Calculate the pixel center of a main-menu option using the same reference layout and uniform
// scale as the screen. Synthetic clicks then exercise the rendered target at different window
// sizes.
Vector2 CenteredOption(int Index) {
    const float Factor = std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
    return {GetScreenWidth() * .5f,
            (GetScreenHeight() - 720 * Factor) * .5f + (366 + Index * 76) * Factor};
}

// Exercise route requests, invalid durations, completion, detail motion, fades, and backward
// navigation in pure MenuJourney state. Verify an active trip cannot be replaced by a second
// request midway through travel.
void CheckJourney() {
    MenuJourney Path;
    ExpectMenu(Path.GoTo(MenuPlace::Players), "Play must start a trip to player selection");
    ExpectMenu(!Path.GoTo(MenuPlace::Controls), "a second pick must not replace a trip mid-turn");
    for (float CurrentInvalid : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
        Path.Update(CurrentInvalid);
        ExpectMenu(Path.Progress() == 0, "invalid delta must not advance corridor travel");
    }
    Path.Update(.25f);
    ExpectMenu(Path.Moving() && Path.UiOpacity() < .01f && !Path.DestinationVisible(),
               "the old interface must fade before the corridor turns");
    Path.Update(.25f);
    ExpectMenu(Path.DestinationVisible() && Path.UiOpacity() == 0,
               "screen handoff must occur while the interface is hidden");
    Path.Update(.40f);
    ExpectMenu(!Path.Moving() && Path.To() == MenuPlace::Players && Path.UiOpacity() == 1,
               "arrival must retain its new corridor stop with a visible interface");
    ExpectMenu(Path.GoTo(MenuPlace::Levels) && Path.Turn() == 1,
               "selecting a player must take the right-hand gallery");
    Path.Update(1);
    ExpectMenu(Path.GoTo(MenuPlace::Preparing) && Path.Turn() == -1,
               "selecting a level must turn left toward the inner doorway");
    Path.Update(1);
    ExpectMenu(Path.GoTo(MenuPlace::Levels) && Path.Turn() == 1 && Path.Backwards(),
               "backtracking must reverse the previous turn");
    Path.Update(1);
    ExpectMenu(Path.GoTo(MenuPlace::Players) && Path.Turn() == -1 && Path.Backwards(),
               "Back from level selection must retrace the right-hand gallery");
    Path.Update(1);
    Path.GoTo(MenuPlace::Entrance);
    Path.Update(1);
    ExpectMenu(Path.GoTo(MenuPlace::Controls) && Path.Turn() == -1,
               "Controls must enter the left-hand branch");
    Path.Update(1);
    ExpectMenu(Path.GoTo(MenuPlace::Entrance) && Path.Turn() == 1,
               "returning from Controls must reverse its left turn");
    Path.Update(1);
    Path.Snap(MenuPlace::Levels);
    for (int Repeat = 0; Repeat < 3; ++Repeat) {
        ExpectMenu(Path.ShowDetail(true), "a custom choice must move within its corridor stop");
        Path.Update(.19f);
        ExpectMenu(Path.DetailAmount() > .49f && Path.DetailAmount() < .51f &&
                       Path.UiOpacity() == 1,
                   "detail choices must move smoothly while keeping their input readable");
        ExpectMenu(!Path.ShowDetail(false),
                   "a detail trip must ignore repeated choices until arrival");
        Path.Update(.20f);
        ExpectMenu(Path.DetailAmount() == 1 && !Path.ShowDetail(true),
                   "repeated confirmation must not accumulate camera zoom");
        Path.ShowDetail(false);
        Path.Update(.40f);
        ExpectMenu(Path.DetailAmount() == 0 && Path.To() == MenuPlace::Levels,
                   "cancelling a detail must restore the same corridor viewpoint");
    }
    Path.Snap(MenuPlace::Entrance);
    Path.GoTo(MenuPlace::Exit);
    Path.Update(1);
    ExpectMenu(!Path.Moving() && Path.UiOpacity() == 0,
               "Quit must finish its trip with the interface hidden");
}
}

// Check journey behavior, scaled option targets, keyboard-pointer interaction, and packaged menu
// artwork in the hidden UI graphics context. Synthetic input avoids changing desktop focus or
// sending real clicks.
void RunMainMenuTests() {
    CheckJourney();
    MainMenu CurrentMenu;
    MenuBackground Background;
    Background.Snap(MenuPlace::Entrance);
    ExpectMenu(CurrentMenu.Update() == Screen::MainMenu, "idle menu must remain open");
    for (int Index : {1, 2}) {
        MenuSelection = Index;
        MenuConfirm = true;
        ExpectMenu(CurrentMenu.Update() == (Index == 1 ? Screen::CONTROLS : Screen::QUIT),
                   "keyboard controls and quit must remain available");
    }
    // A stationary pointer over Play must not override the keyboard's chosen action.
    MenuPosition = CenteredOption(0);
    MenuSelection = 2;
    ExpectMenu(CurrentMenu.Update() == Screen::QUIT, "stationary mouse must preserve keyboard selection");
    MenuSelection = 0;
    MenuConfirm = true;
    ExpectMenu(CurrentMenu.Update() == Screen::PlayerSelect,
               "Play must request player selection for the shared journey controller");
    MenuConfirm = false;
    MenuClick = false;
    MenuSelection = -1;
    ExpectMenu(CurrentMenu.Update() == Screen::MainMenu,
               "returning to the main menu must reset the completed transition");
    MenuClick = true;
    MenuPosition = CenteredOption(1);
    ExpectMenu(CurrentMenu.Update() == Screen::CONTROLS,
               "the visually centered Controls button must accept clicks");
    MenuPosition = {-100, -100};
    MenuClick = false;
    CurrentMenu.Update();
    RenderTexture2D Canvas = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    BeginTextureMode(Canvas);
    CurrentMenu.Draw();
    EndTextureMode();
    EndDrawing();
    Image Preview = LoadImageFromTexture(Canvas.texture);
    const Color CurrentBackdrop = GetImageColor(Preview, 20, Preview.height / 2);
    UnloadImage(Preview);
    UnloadRenderTexture(Canvas);
    ExpectMenu(CurrentBackdrop.r != MenuTheme::Ink.r || CurrentBackdrop.g != MenuTheme::Ink.g ||
                   CurrentBackdrop.b != MenuTheme::Ink.b,
               "the corridor artwork must load from the packaged assets directory");
    MenuSelection = -1;
    MenuConfirm = MenuClick = false;
    MenuPosition = {-100, -100};
}
