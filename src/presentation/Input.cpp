#include "Input.h"
#include "raylib.h"

InputState readInput() {
    InputState in;
    in.forward = (float)(IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        - (float)(IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN));
    in.right = (float)(IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        - (float)(IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
    Vector2 md = GetMouseDelta();
    in.mouseDX = md.x;
    in.mouseDY = md.y;
    in.dt = GetFrameTime();
    in.gravityToggle = IsKeyPressed(KEY_G);
    return in;
}

void updateMenuSelection(int& selected, int count) {
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % count;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + count - 1) % count;
}

bool menuConfirmed() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE);
}

bool editText(std::string& text, int maxLength, bool (*allowed)(int)) {
    bool changed = false;
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if (allowed(ch) && (int)text.size() < maxLength) {
            text += (char)ch;
            changed = true;
        }
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !text.empty()) {
        text.pop_back();
        changed = true;
    }
    return changed;
}
