#include "presentation/Application.h"
#include "raylib.h"

// Create the resizable window and graphics context, load an available icon, and run the
// application until it exits. Keep Application in an inner scope so its textures, fonts, and
// shaders are destroyed before CloseWindow releases the context.
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "T1");

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

    {
        Application application;
        application.run();
    } 

    CloseWindow();
}
