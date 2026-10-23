#include "MainMenu.h"
#include "Ui.h"
#include <cmath>

static const int COUNT = 3;
static const char* LABELS[COUNT] = {"Play", "Controls", "Quit"};

static void drawNeonGrid(float time) {
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    float horizon = height * 0.55f;
    float center = width * 0.5f;

    DrawRectangleGradientV(0, 0, width, (int)horizon + 2,
                           Color{55, 22, 126, 255}, Color{241, 37, 147, 255});
    DrawRectangleGradientV(0, (int)horizon, width, height - (int)horizon,
                           Color{92, 12, 83, 255}, Color{5, 3, 20, 255});

    DrawTriangle({0, horizon + 2.0f}, {width * 0.25f, horizon - 15.0f},
                 {width * 0.51f, horizon + 2.0f}, Color{221, 24, 119, 255});
    DrawTriangle({width * 0.39f, horizon + 2.0f}, {width * 0.69f, horizon - 9.0f},
                 {(float)width, horizon + 2.0f}, Color{225, 25, 123, 255});

    for (int i = -12; i <= 12; i++) {
        float spread = (float)i / 12.0f;
        float bottomX = center + spread * width * 0.78f;
        unsigned char alpha = (unsigned char)(40 + std::fabs(spread) * 56.0f);
        DrawLineEx({center, horizon}, {bottomX, (float)height}, 5.0f,
                   Color{255, 0, 205, (unsigned char)(alpha / 4)});
        DrawLineEx({center, horizon}, {bottomX, (float)height}, 1.0f,
                   Color{255, 42, 213, alpha});
    }

    float phase = std::fmod(time * 0.9f, 1.0f);
    for (int i = 0; i <= 18; i++) {
        float depth = ((float)i + phase) / 18.0f;
        if (depth > 1.0f) continue;
        float y = horizon + (height - horizon) * depth * depth;
        unsigned char alpha = (unsigned char)(28 + depth * 98.0f);
        DrawLineEx({0, y}, {(float)width, y}, 4.0f,
                   Color{255, 0, 205, (unsigned char)(alpha / 4)});
        DrawLineEx({0, y}, {(float)width, y}, 1.0f,
                   Color{255, 42, 213, alpha});
    }

    float pulse = 0.82f + 0.18f * std::sin(time * 2.8f);
    DrawLineEx({0, horizon}, {(float)width, horizon}, 10.0f,
               Color{255, 40, 170, (unsigned char)(35.0f * pulse)});
    DrawLineEx({0, horizon}, {(float)width, horizon}, 2.0f,
               Color{255, 105, 225, (unsigned char)(200.0f * pulse)});
}

MainMenu::MainMenu() : selected(0) {}

Screen MainMenu::update() {
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) selected = (selected + 1) % COUNT;
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) selected = (selected + COUNT - 1) % COUNT;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        if (selected == 0) return Screen::PLAYER_SELECT;
        if (selected == 1) return Screen::CONTROLS;
        return Screen::QUIT;
    }
    return Screen::MAIN_MENU;
}

void MainMenu::draw() const {
    float time = (float)GetTime();
    drawNeonGrid(time);

    int titleY = uiY(100 + (int)(std::sin(time * 1.4f) * 3.0f));
    int titleSize = uiSize(90);
    const char* title = "MAZE";
    int titleX = (GetScreenWidth() - MeasureText(title, titleSize)) / 2;
    DrawText(title, titleX + uiSize(3), titleY + uiSize(3), titleSize, Fade(MAGENTA, 0.38f));
    DrawText(title, titleX, titleY, titleSize, GOLD);
    drawCentered("A 3D maze game", 200, 26, Color{170, 225, 245, 255});

    for (int i = 0; i < COUNT; i++) {
        int y = uiY(300 + i * 70);
        int textSize = uiSize(40);
        int textWidth = MeasureText(LABELS[i], textSize);
        int textX = (GetScreenWidth() - textWidth) / 2;
        Color color = i == selected ? GOLD : GRAY;
        DrawText(LABELS[i], textX, y, textSize, color);

        if (i == selected) {
            float shift = std::sin(time * 4.0f) * uiSize(3);
            float tipX = (float)textX - uiSize(8) + shift;
            DrawTriangle(
                {tipX, (float)y + uiSize(20)},
                {tipX - uiSize(12), (float)y + uiSize(12)},
                {tipX - uiSize(12), (float)y + uiSize(28)},
                GOLD);
        }
    }

    drawCentered("UP/DOWN choose    ENTER select", 620, 24, LIGHTGRAY);
}