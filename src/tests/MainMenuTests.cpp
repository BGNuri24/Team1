#include "../presentation/Input.h"
#include "../presentation/MainMenu.h"
#include "../presentation/Ui.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Drive menu input without sending keys or moving the desktop pointer.
int menuSelection = -1;
bool menuConfirm = false, menuClick = false;
Vector2 menuPosition{-100, -100}, menuMovement{};
double menuTime() {
    return 4;
}
Vector2 menuMousePosition() {
    return menuPosition;
}
Vector2 menuMouseDelta() {
    return menuMovement;
}
bool menuMousePressed(int) {
    return menuClick;
}
bool menuConfirmation() {
    return menuConfirm;
}
void menuChoose(int& selected, int count) {
    if (menuSelection >= 0)
        selected = std::clamp(menuSelection, 0, count - 1);
}
}

#define GetTime menuTime
#define GetMousePosition menuMousePosition
#define GetMouseDelta menuMouseDelta
#define IsMouseButtonPressed menuMousePressed
#define menuConfirmed menuConfirmation
#define updateMenuSelection menuChoose
#include "../presentation/MainMenu.cpp"
#undef GetTime
#undef GetMousePosition
#undef GetMouseDelta
#undef IsMouseButtonPressed
#undef menuConfirmed
#undef updateMenuSelection

namespace {
void expectMenu(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

Vector2 centeredOption(int index) {
    const float factor = std::min(GetScreenWidth() / 1280.0f, GetScreenHeight() / 720.0f);
    return {GetScreenWidth() * .5f,
            (GetScreenHeight() - 720 * factor) * .5f + (366 + index * 76) * factor};
}

void checkJourney() {
    MenuJourney path;
    expectMenu(path.goTo(MenuPlace::Players), "Play must start a trip to player selection");
    expectMenu(!path.goTo(MenuPlace::Controls), "a second pick must not replace a trip mid-turn");
    for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(),
                          std::numeric_limits<float>::infinity()}) {
        path.update(invalid);
        expectMenu(path.progress() == 0, "invalid delta must not advance corridor travel");
    }
    path.update(.25f);
    expectMenu(path.moving() && path.uiOpacity() < .01f && !path.destinationVisible(),
               "the old interface must fade before the corridor turns");
    path.update(.25f);
    expectMenu(path.destinationVisible() && path.uiOpacity() == 0,
               "screen handoff must occur while the interface is hidden");
    path.update(.40f);
    expectMenu(!path.moving() && path.to() == MenuPlace::Players && path.uiOpacity() == 1,
               "arrival must retain its new corridor stop with a visible interface");
    expectMenu(path.goTo(MenuPlace::Levels) && path.turn() == 1,
               "selecting a player must take the right-hand gallery");
    path.update(1);
    expectMenu(path.goTo(MenuPlace::Preparing) && path.turn() == -1,
               "selecting a level must turn left toward the inner doorway");
    path.update(1);
    expectMenu(path.goTo(MenuPlace::Levels) && path.turn() == 1 && path.backwards(),
               "backtracking must reverse the previous turn");
    path.update(1);
    expectMenu(path.goTo(MenuPlace::Players) && path.turn() == -1 && path.backwards(),
               "Back from level selection must retrace the right-hand gallery");
    path.update(1);
    path.goTo(MenuPlace::Entrance);
    path.update(1);
    expectMenu(path.goTo(MenuPlace::Controls) && path.turn() == -1,
               "Controls must enter the left-hand branch");
    path.update(1);
    expectMenu(path.goTo(MenuPlace::Entrance) && path.turn() == 1,
               "returning from Controls must reverse its left turn");
    path.update(1);
    path.snap(MenuPlace::Levels);
    for (int repeat = 0; repeat < 3; ++repeat) {
        expectMenu(path.showDetail(true), "a custom choice must move within its corridor stop");
        path.update(.19f);
        expectMenu(path.detailAmount() > .49f && path.detailAmount() < .51f &&
                       path.uiOpacity() == 1,
                   "detail choices must move smoothly while keeping their input readable");
        expectMenu(!path.showDetail(false),
                   "a detail trip must ignore repeated choices until arrival");
        path.update(.20f);
        expectMenu(path.detailAmount() == 1 && !path.showDetail(true),
                   "repeated confirmation must not accumulate camera zoom");
        path.showDetail(false);
        path.update(.40f);
        expectMenu(path.detailAmount() == 0 && path.to() == MenuPlace::Levels,
                   "cancelling a detail must restore the same corridor viewpoint");
    }
    path.snap(MenuPlace::Entrance);
    path.goTo(MenuPlace::Exit);
    path.update(1);
    expectMenu(!path.moving() && path.uiOpacity() == 0,
               "Quit must finish its trip with the interface hidden");
}
}

// Called in the UI suite's hidden graphics context. Check the changed navigation behavior, the
// centered click targets and the packaged artwork rather than synthesizing real desktop input.
void runMainMenuTests() {
    checkJourney();
    MainMenu menu;
    MenuBackground background;
    background.snap(MenuPlace::Entrance);
    expectMenu(menu.update() == Screen::MAIN_MENU, "idle menu must remain open");
    for (int index : {1, 2}) {
        menuSelection = index;
        menuConfirm = true;
        expectMenu(menu.update() == (index == 1 ? Screen::CONTROLS : Screen::QUIT),
                   "keyboard controls and quit must remain available");
    }
    // A stationary pointer over Play must not override the keyboard's chosen action.
    menuPosition = centeredOption(0);
    menuSelection = 2;
    expectMenu(menu.update() == Screen::QUIT, "stationary mouse must preserve keyboard selection");
    menuSelection = 0;
    menuConfirm = true;
    expectMenu(menu.update() == Screen::PLAYER_SELECT,
               "Play must request player selection for the shared journey controller");
    menuConfirm = false;
    menuClick = false;
    menuSelection = -1;
    expectMenu(menu.update() == Screen::MAIN_MENU,
               "returning to the main menu must reset the completed transition");
    menuClick = true;
    menuPosition = centeredOption(1);
    expectMenu(menu.update() == Screen::CONTROLS,
               "the visually centered Controls button must accept clicks");
    menuPosition = {-100, -100};
    menuClick = false;
    menu.update();
    RenderTexture2D canvas = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    BeginDrawing();
    BeginTextureMode(canvas);
    menu.draw();
    EndTextureMode();
    EndDrawing();
    Image preview = LoadImageFromTexture(canvas.texture);
    const Color backdrop = GetImageColor(preview, 20, preview.height / 2);
    UnloadImage(preview);
    UnloadRenderTexture(canvas);
    expectMenu(backdrop.r != MenuTheme::Ink.r || backdrop.g != MenuTheme::Ink.g ||
                   backdrop.b != MenuTheme::Ink.b,
               "the corridor artwork must load from the packaged assets directory");
    menuSelection = -1;
    menuConfirm = menuClick = false;
    menuPosition = {-100, -100};
}
