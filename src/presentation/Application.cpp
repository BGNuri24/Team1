#include "Application.h"
#include "Ui.h"
#include "raylib.h"
#include <optional>

namespace {
// Identify screens that participate in animated corridor menu navigation. Gameplay, pause,
// results, generation, and quit use separate handling rather than being treated as ordinary menu
// pages.
bool MenuScreen(Screen CurrentScreen) {
    return CurrentScreen == Screen::MainMenu || CurrentScreen == Screen::CONTROLS ||
           CurrentScreen == Screen::PlayerSelect || CurrentScreen == Screen::LevelSelect;
}

// Translate an application screen into its corridor-art destination. Return an empty optional for
// screens without a menu stop so gameplay and overlays do not trigger unrelated backdrop
// navigation.
std::optional<MenuPlace> CorridorPlace(Screen CurrentScreen) {
    switch (CurrentScreen) {
    case Screen::MainMenu:
        return MenuPlace::Entrance;
    case Screen::CONTROLS:
        return MenuPlace::Controls;
    case Screen::PlayerSelect:
        return MenuPlace::Players;
    case Screen::LevelSelect:
        return MenuPlace::Levels;
    case Screen::GENERATING:
        return MenuPlace::Preparing;
    case Screen::QUIT:
        return MenuPlace::Exit;
    default:
        return std::nullopt;
    }
}
}

// Connect profile selection to shared players and pause controls to shared settings, then
// initialize run parameters and the main-menu state. Prepare the active palette and place the
// shared corridor at its entrance.
Application::Application()
    : CurrentPlayerSelect(CurrentPlayers), CurrentPauseMenu(CurrentSettings), CurrentSize(0), CurrentSeconds(0),
      CurrentMode(GameMode::Normal), BestTime(-1.0f), NewBestTime(false),
      CurrentScreen(Screen::MainMenu) {
    PrepareTheme();
    MenuBackdrop.Snap(MenuPlace::Entrance);
}

// Keep the menu palette synchronized with ActiveMazeTheme, applying it only when the selected
// theme differs. This avoids unnecessary theme changes while menus are repeatedly updated.
void Application::PrepareTheme() {
    if (MenuTheme::Current() != ActiveMazeTheme)
        MenuTheme::Apply(ActiveMazeTheme);
}

// Dispatch frames until a screen requests Quit or the operating system requests window closure.
// Each handler returns the next application state and performs that screen's update and drawing.
void Application::Run() {
    while (CurrentScreen != Screen::QUIT && !WindowShouldClose()) {
        CurrentScreen = UpdateCurrentScreen();
    }
}

// Dispatch one frame to the active state, giving an in-progress corridor journey priority over
// normal screen input. Route menu changes through travel animation and snap returns from gameplay
// to the correct menu stop.
Screen Application::UpdateCurrentScreen() {
    if (!CurrentGame)
        PrepareTheme();
    if (MenuBackdrop.Moving())
        return UpdateMenuTravel();
    // Dispatch the current screen through an immediately invoked lambda and retain its requested
    // destination. The outer function can then apply shared corridor-navigation rules to every
    // handler's result.
    const Screen Next = [&] {
        switch (CurrentScreen) {
        case Screen::MainMenu:
            return UpdateMainMenu();
        case Screen::CONTROLS:
            return UpdateControls();
        case Screen::PlayerSelect:
            return UpdatePlayerSelect();
        case Screen::LevelSelect:
            return UpdateLevelSelect();
        case Screen::GENERATING:
            return UpdateGenerating();
        case Screen::PLAYING:
            return UpdatePlaying();
        case Screen::PAUSED:
            return UpdatePaused();
        case Screen::END:
            return UpdateEnd();
        case Screen::QUIT:
            return Screen::QUIT;
        }
        return Screen::QUIT;
    }();
    const std::optional<MenuPlace> Destination = CorridorPlace(Next);
    if (MenuScreen(CurrentScreen)) {
        if (Next != CurrentScreen && Destination && MenuBackdrop.Navigate(*Destination)) {
            PendingMenu = Next;
            TravelFirstFrame = true;
            return CurrentScreen;
        }
        // Name/custom-size dialogs make a smaller move within their current corridor stop.
        if (MenuBackdrop.Moving()) {
            PendingMenu = Next;
            TravelFirstFrame = true;
            return CurrentScreen;
        }
    } else if (Destination && Next != CurrentScreen) {
        // Returning from a paused or completed run starts at the correct menu stop.
        MenuBackdrop.Snap(*Destination);
    }
    return Next;
}
