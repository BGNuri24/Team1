#include "raylib.h"
#include "presentation/Application.h"
#include <cstdlib>
#include <ctime>

// Create the resizable raylib window, run the application, and destroy GPU resources before closing
// the graphics context.
int main() {
    SetConfigFlags(FLAG_FULLSCREEN_MODE | FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 700, "Maze");

    Image appIcon = LoadImage("T1_.png");
    if (appIcon.data == nullptr) {
        appIcon = LoadImage("docs/assets/T1_.png");
    }
    if (appIcon.data != nullptr) {
        SetWindowIcon(appIcon);
        UnloadImage(appIcon);
    }
    SetWindowMinSize(640, 480);
    SetExitKey(KEY_NULL);
    SetTargetFPS(120);
    srand((unsigned)time(0));

    {
        Application application;
        application.run();
    }

    CloseWindow();
}
