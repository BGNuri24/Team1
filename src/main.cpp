#include "raylib.h"
#include "presentation/Application.h"
#include <cstdlib>
#include <ctime>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 700, "Maze");
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
