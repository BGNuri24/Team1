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
void runMapPeekTests();

// Supply deterministic keyboard, text, mouse and timing events through raylib stubs without opening a
// window.
static std::array<bool, 512> pressed{}, repeated{}, held{};
static std::deque<int> characters;
static float wheel = 0;
static bool focused = true, minimized = false;
// Return the fixture's synthetic focus flag instead of observing the actual desktop window. Tests
// can request pause behavior deterministically without moving focus away from the user's
// applications.
bool IsWindowFocused() { return focused; }
// Return the fixture's synthetic minimized state without minimizing a real window. Together with
// IsWindowFocused, this controls whether gameplay input is considered active.
bool IsWindowMinimized() { return minimized; }
// Read the synthetic initial-press flag for a key rather than querying the keyboard. This lets
// assertions distinguish one-frame action requests from held movement input.
bool IsKeyPressed(int key) { return pressed.at(key); }
// Read the fixture's synthetic repeat flag for a key. Settings and text-entry checks can exercise
// held-key repetition without depending on operating-system key timing.
bool IsKeyPressedRepeat(int key) { return repeated.at(key); }
// Return the fixture's synthetic held-key state for movement mappings. Tests can combine opposing
// directions and verify InputState without requiring real key presses.
bool IsKeyDown(int key) { return held.at(key); }
// Pop one queued synthetic character and return zero when the queue is empty. Text-entry tests
// supply accepted and rejected code points through the same interface used by real input.
int GetCharPressed() {
    if (characters.empty()) return 0;
    int ch = characters.front();
    characters.pop_front();
    return ch;
}
// Return a fixed frame duration in seconds for deterministic input assertions. Gameplay timing
// tests therefore do not depend on the machine's rendering speed.
float GetFrameTime() { return 0.125f; }
// Return the fixture's fixed two-axis mouse displacement. Sensitivity tests compare this source
// delta with the scaled InputState values without moving the physical pointer.
Vector2 GetMouseDelta() { return {12, -8}; }
// Return the fixture's synthetic wheel event for scroll-based jump checks. The test can
// independently control wheel input and Space presses without consuming real device events.
float GetMouseWheelMove() { return wheel; }

// Throw a descriptive exception when an input expectation fails, keeping assertions active in
// every build configuration. main turns that exception into a failing process result.
static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Run synthetic menu, editing, movement, sensitivity, focus, jump, and map-timing checks. Report
// the first failure through a nonzero exit code without requiring real desktop input.
int main() {
    try {
        runMapPeekTests();
        int selected = 0;
        pressed[KEY_UP] = true;
        updateMenuSelection(selected, 4);
        require(selected == 3, "up should wrap to last option");
        pressed[KEY_UP] = false;
        pressed[KEY_DOWN] = pressed[KEY_S] = true;
        updateMenuSelection(selected, 4);
        require(selected == 0, "down and S together must move only once");
        pressed[KEY_UP] = true;
        updateMenuSelection(selected, 4);
        require(selected == 0, "opposing presses should cancel");
        updateMenuSelection(selected, 1);
        require(selected == 0, "single-option menu should remain valid");
        selected = 2;
        updateMenuSelection(selected, 0);
        require(selected == 2, "empty menus should leave their selection unchanged");
        pressed.fill(false);
        require(!menuConfirmed(), "idle menu should not confirm");
        pressed[KEY_SPACE] = true;
        require(menuConfirmed(), "space should confirm menu selection");
        pressed.fill(false);
        pressed[KEY_ENTER] = true;
        require(menuConfirmed(), "enter should confirm menu selection");
        pressed.fill(false);

        std::string text;
        characters = {'A', '-', '_', 0x0410, '1', 'Z'};
        require(editText(text, 3, isNameCharacter) && text == "A_1",
                "entry should filter characters and enforce length");
        characters = {'x'};
        require(!editText(text, 3, isNameCharacter), "full entry should remain unchanged");
        repeated[KEY_BACKSPACE] = true;
        require(editText(text, 3, isNameCharacter) && text == "A_",
                "held backspace should remove one character");
        repeated.fill(false);
        text.clear();
        pressed[KEY_BACKSPACE] = true;
        require(!editText(text, 3, isNameCharacter), "backspace on empty entry should be harmless");
        pressed.fill(false);
        characters = {'a', '5', '0', '9'};
        // Filter synthetic custom-size input to decimal digits only. The test verifies rejected
        // characters do not occupy the limited text buffer.
        editText(text, 2, [](int ch) { return ch >= '0' && ch <= '9'; });
        require(text == "50", "maze-size entry should accept only two digits");

        held[KEY_W] = held[KEY_UP] = held[KEY_S] = held[KEY_D] = true;
        pressed[KEY_G] = true;
        InputState input = readInput();
        require(input.forward == 0 && input.right == 1 && input.gravityToggle &&
                input.mouseDX == 12 && input.mouseDY == -8 && input.dt == 0.125f,
                "movement, mouse and gravity input must retain their mappings");

        // Read synthetic mouse input with the supplied sensitivity and compare both resulting
        // axes against expected values. Reuse this assertion for normal, clamped, and non-finite
        // multipliers.
        auto requireSensitivity = [&](float multiplier, float expectedX, float expectedY) {
            InputState scaled = readInput(multiplier);
            require(scaled.mouseDX == expectedX && scaled.mouseDY == expectedY,
                "sensitivity must scale horizontal and vertical mouse look together");
            require(scaled.forward == input.forward && scaled.right == input.right &&
                scaled.dt == input.dt && scaled.gravityToggle == input.gravityToggle,
                "sensitivity must not affect movement, timing or gravity controls");
        };
        requireSensitivity(0.5f, 6, -4);
        requireSensitivity(2.0f, 24, -16);
        requireSensitivity(0.25f, 3, -2);
        requireSensitivity(3.0f, 36, -24);
        requireSensitivity(-10.0f, 3, -2);
        requireSensitivity(10.0f, 36, -24);
        requireSensitivity(std::numeric_limits<float>::quiet_NaN(), 12, -8);
        requireSensitivity(std::numeric_limits<float>::infinity(), 12, -8);
        requireSensitivity(-std::numeric_limits<float>::infinity(), 12, -8);
        held[KEY_SPACE] = true;
        require(!readInput().jumpPressed, "holding space must not automatically bunny hop");
        pressed[KEY_SPACE] = true;
        require(readInput().jumpPressed, "a fresh space press must request a jump");
        pressed[KEY_SPACE] = false;
        wheel = -1;
        require(readInput().jumpPressed, "wheel down must request a jump");
        wheel = 1;
        require(readInput().jumpPressed, "wheel up must request a jump");
        wheel = 0;
        require(!readInput().jumpPressed, "jump input must clear between presses");
        held[KEY_E] = true;
        require(!readInput().mapPressed, "holding E must not repeatedly request the map");
        pressed[KEY_E] = true;
        require(readInput().mapPressed, "a fresh E press must request a map peek");
        pressed[KEY_E] = false;
        require(!readInput().mapPressed, "map requests must clear after the initial press");
        held[KEY_E] = false;
        pressed[KEY_M] = true;
        require(!readInput().mapPressed, "M must no longer request the map");
        pressed[KEY_M] = false;
        require(gameplayWindowActive() && !gameplayPauseRequested(),
                "an active idle window must allow gameplay");
        pressed[KEY_ESCAPE] = true;
        require(gameplayPauseRequested(), "Escape must request a pause before gameplay input is consumed");
        pressed[KEY_ESCAPE] = false;
        focused = false;
        require(!gameplayWindowActive() && gameplayPauseRequested(), "losing focus must pause the run");
        focused = true;
        minimized = true;
        require(!gameplayWindowActive() && gameplayPauseRequested(), "minimization must pause even if focus remains reported");
        minimized = false;
        require(gameplayWindowActive() && !gameplayPauseRequested(), "restored focus must permit an explicit resume");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Input tests passed.\n";
}
