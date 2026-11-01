#include "../presentation/Input.h"
#include "../data/Validation.h"
#include <array>
#include <deque>
#include <iostream>
#include <limits>
#include <stdexcept>

// Feed deterministic keyboard events without requiring a window or taking focus.
static std::array<bool, 512> pressed{}, repeated{}, held{};
static std::deque<int> characters;
static float wheel = 0;
bool IsKeyPressed(int key) { return pressed.at(key); }
bool IsKeyPressedRepeat(int key) { return repeated.at(key); }
bool IsKeyDown(int key) { return held.at(key); }
int GetCharPressed() {
    if (characters.empty()) return 0;
    int ch = characters.front();
    characters.pop_front();
    return ch;
}
float GetFrameTime() { return 0.125f; }
Vector2 GetMouseDelta() { return {12, -8}; }
float GetMouseWheelMove() { return wheel; }

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
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
        editText(text, 2, [](int ch) { return ch >= '0' && ch <= '9'; });
        require(text == "50", "maze-size entry should accept only two digits");

        held[KEY_W] = held[KEY_UP] = held[KEY_S] = held[KEY_D] = true;
        pressed[KEY_G] = true;
        InputState input = readInput();
        require(input.forward == 0 && input.right == 1 && input.gravityToggle &&
                input.mouseDX == 12 && input.mouseDY == -8 && input.dt == 0.125f,
                "movement, mouse and gravity input must retain their mappings");

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
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "Input tests passed.\n";
}
