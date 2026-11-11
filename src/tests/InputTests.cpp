#include "../presentation/Input.h"
#include "../data/Validation.h"
#include <array>
#include <deque>
#include <iostream>
#include <limits>
#include <stdexcept>
// Check map duration, cooldown, repeated presses, large frame intervals, and new-run resets for
// supported sizes and modes. Drive pure state directly so the results are independent of
// rendering speed.
void RunMapPeekTests();

// Supply deterministic keyboard, text, mouse and timing events through raylib stubs without opening a
// window.
static std::array<bool, 512> Pressed{}, CurrentRepeated{}, CurrentHeld{};
static std::deque<int> Characters;
static float Wheel = 0;
static bool Focused = true, Minimized = false;
// Return the fixture's synthetic focus flag instead of observing the actual desktop window. Tests
// can request pause behavior deterministically without moving focus away from the user's
// applications.
bool IsWindowFocused() { return Focused; }
// Return the fixture's synthetic minimized state without minimizing a real window. Together with
// IsWindowFocused, this controls whether gameplay input is considered active.
bool IsWindowMinimized() { return Minimized; }
// Read the synthetic initial-press flag for a key rather than querying the keyboard. This lets
// assertions distinguish one-frame action requests from held movement input.
bool IsKeyPressed(int Key) { return Pressed.at(Key); }
// Read the fixture's synthetic repeat flag for a key. Settings and text-entry checks can exercise
// held-key repetition without depending on operating-system key timing.
bool IsKeyPressedRepeat(int Key) { return CurrentRepeated.at(Key); }
// Return the fixture's synthetic held-key state for movement mappings. Tests can combine opposing
// directions and verify InputState without requiring real key presses.
bool IsKeyDown(int Key) { return CurrentHeld.at(Key); }
// Pop one queued synthetic character and return zero when the queue is empty. Text-entry tests
// supply accepted and rejected code points through the same interface used by real input.
int GetCharPressed() {
    if (Characters.empty()) return 0;
    int Ch = Characters.front();
    Characters.pop_front();
    return Ch;
}
// Return a fixed frame duration in seconds for deterministic input assertions. Gameplay timing
// tests therefore do not depend on the machine's rendering speed.
float GetFrameTime() { return 0.125f; }
// Return the fixture's fixed two-axis mouse displacement. Sensitivity tests compare this source
// delta with the scaled InputState values without moving the physical pointer.
Vector2 GetMouseDelta() { return {12, -8}; }
// Return the fixture's synthetic wheel event for scroll-based jump checks. The test can
// independently control wheel input and Space presses without consuming real device events.
float GetMouseWheelMove() { return Wheel; }

// Throw a descriptive exception when an input expectation fails, keeping assertions active in
// every build configuration. main turns that exception into a failing process result.
static void Require(bool Condition, const char* Message) {
    if (!Condition) throw std::runtime_error(Message);
}

// Run synthetic menu, editing, movement, sensitivity, focus, jump, and map-timing checks. Report
// the first failure through a nonzero exit code without requiring real desktop input.
int main() {
    try {
        RunMapPeekTests();
        int CurrentSelected = 0;
        Pressed[KEY_UP] = true;
        UpdateMenuSelection(CurrentSelected, 4);
        Require(CurrentSelected == 3, "up should wrap to last option");
        Pressed[KEY_UP] = false;
        Pressed[KEY_DOWN] = Pressed[KEY_S] = true;
        UpdateMenuSelection(CurrentSelected, 4);
        Require(CurrentSelected == 0, "down and S together must move only once");
        Pressed[KEY_UP] = true;
        UpdateMenuSelection(CurrentSelected, 4);
        Require(CurrentSelected == 0, "opposing presses should cancel");
        UpdateMenuSelection(CurrentSelected, 1);
        Require(CurrentSelected == 0, "single-option menu should remain valid");
        CurrentSelected = 2;
        UpdateMenuSelection(CurrentSelected, 0);
        Require(CurrentSelected == 2, "empty menus should leave their selection unchanged");
        Pressed.fill(false);
        Require(!MenuConfirmed(), "idle menu should not confirm");
        Pressed[KEY_SPACE] = true;
        Require(MenuConfirmed(), "space should confirm menu selection");
        Pressed.fill(false);
        Pressed[KEY_ENTER] = true;
        Require(MenuConfirmed(), "enter should confirm menu selection");
        Pressed.fill(false);

        std::string CurrentText;
        Characters = {'A', '-', '_', 0x0410, '1', 'Z'};
        Require(EditText(CurrentText, 3, IsNameCharacter) && CurrentText == "A_1",
                "entry should filter characters and enforce length");
        Characters = {'x'};
        Require(!EditText(CurrentText, 3, IsNameCharacter), "full entry should remain unchanged");
        CurrentRepeated[KEY_BACKSPACE] = true;
        Require(EditText(CurrentText, 3, IsNameCharacter) && CurrentText == "A_",
                "held backspace should remove one character");
        CurrentRepeated.fill(false);
        CurrentText.clear();
        Pressed[KEY_BACKSPACE] = true;
        Require(!EditText(CurrentText, 3, IsNameCharacter), "backspace on empty entry should be harmless");
        Pressed.fill(false);
        Characters = {'a', '5', '0', '9'};
        // Filter synthetic custom-size input to decimal digits only. The test verifies rejected
        // characters do not occupy the limited text buffer.
        EditText(CurrentText, 2, [](int Ch) { return Ch >= '0' && Ch <= '9'; });
        Require(CurrentText == "50", "maze-size entry should accept only two digits");

        CurrentHeld[KEY_W] = CurrentHeld[KEY_UP] = CurrentHeld[KEY_S] = CurrentHeld[KEY_D] = true;
        Pressed[KEY_G] = true;
        InputState CurrentInput = ReadInput();
        Require(CurrentInput.CurrentForward == 0 && CurrentInput.CurrentRight == 1 && CurrentInput.GravityToggle &&
                CurrentInput.MouseDx == 12 && CurrentInput.MouseDy == -8 && CurrentInput.Dt == 0.125f,
                "movement, mouse and gravity input must retain their mappings");

        // Read synthetic mouse input with the supplied sensitivity and compare both resulting
        // axes against expected values. Reuse this assertion for normal, clamped, and non-finite
        // multipliers.
        auto RequireSensitivity = [&](float Multiplier, float ExpectedX, float ExpectedY) {
            InputState Scaled = ReadInput(Multiplier);
            Require(Scaled.MouseDx == ExpectedX && Scaled.MouseDy == ExpectedY,
                "sensitivity must scale horizontal and vertical mouse look together");
            Require(Scaled.CurrentForward == CurrentInput.CurrentForward && Scaled.CurrentRight == CurrentInput.CurrentRight &&
                Scaled.Dt == CurrentInput.Dt && Scaled.GravityToggle == CurrentInput.GravityToggle,
                "sensitivity must not affect movement, timing or gravity controls");
        };
        RequireSensitivity(0.5f, 6, -4);
        RequireSensitivity(2.0f, 24, -16);
        RequireSensitivity(0.25f, 3, -2);
        RequireSensitivity(3.0f, 36, -24);
        RequireSensitivity(-10.0f, 3, -2);
        RequireSensitivity(10.0f, 36, -24);
        RequireSensitivity(std::numeric_limits<float>::quiet_NaN(), 12, -8);
        RequireSensitivity(std::numeric_limits<float>::infinity(), 12, -8);
        RequireSensitivity(-std::numeric_limits<float>::infinity(), 12, -8);
        CurrentHeld[KEY_SPACE] = true;
        Require(!ReadInput().JumpPressed, "holding space must not automatically bunny hop");
        Pressed[KEY_SPACE] = true;
        Require(ReadInput().JumpPressed, "a fresh space press must request a jump");
        Pressed[KEY_SPACE] = false;
        Wheel = -1;
        Require(ReadInput().JumpPressed, "wheel down must request a jump");
        Wheel = 1;
        Require(ReadInput().JumpPressed, "wheel up must request a jump");
        Wheel = 0;
        Require(!ReadInput().JumpPressed, "jump input must clear between presses");
        CurrentHeld[KEY_E] = true;
        Require(!ReadInput().MapPressed, "holding E must not repeatedly request the map");
        Pressed[KEY_E] = true;
        Require(ReadInput().MapPressed, "a fresh E press must request a map peek");
        Pressed[KEY_E] = false;
        Require(!ReadInput().MapPressed, "map requests must clear after the initial press");
        CurrentHeld[KEY_E] = false;
        Pressed[KEY_M] = true;
        Require(!ReadInput().MapPressed, "M must no longer request the map");
        Pressed[KEY_M] = false;
        Require(GameplayWindowActive() && !GameplayPauseRequested(),
                "an active idle window must allow gameplay");
        Pressed[KEY_ESCAPE] = true;
        Require(GameplayPauseRequested(), "Escape must request a pause before gameplay input is consumed");
        Pressed[KEY_ESCAPE] = false;
        Focused = false;
        Require(!GameplayWindowActive() && GameplayPauseRequested(), "losing focus must pause the run");
        Focused = true;
        Minimized = true;
        Require(!GameplayWindowActive() && GameplayPauseRequested(), "minimization must pause even if focus remains reported");
        Minimized = false;
        Require(GameplayWindowActive() && !GameplayPauseRequested(), "restored focus must permit an explicit resume");
    } catch (const std::exception& Error) {
        std::cerr << Error.what() << '\n';
        return 1;
    }
    std::cout << "Input tests passed.\n";
}
