#include "../data/MazeGeometry.h"
#include "../presentation/Bloom.h"
#include "../presentation/MazeShaders.h"
#include "rlgl.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Read actual shader output on the GPU, including wall columns on both sides of the 32-bit split.
// The same floor must be visibly lit in an open passage and stay at ambient behind a solid wall.
void checkWallShadows() {
    Shader shader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    require(shader.id && shader.id != rlGetShaderIdDefault(), "lamp shader did not compile");
    auto set = [&](const char* name, const void* value, int type) {
        const int location = GetShaderLocation(shader, name);
        require(location >= 0, "lamp shader is missing a required uniform");
        SetShaderValue(shader, location, value, type);
    };
    const int size = 50;
    const float cellSize = MazeGeometry::CellSize, emission = 0;
    const float roomHeight = MazeGeometry::CeilingHeight;
    const Vector3 warm{1, .72f, .40f}, black{}, down{0, -1, 0};
    set("fixtureColor", &warm, SHADER_UNIFORM_VEC3);
    set("bounceColor", &warm, SHADER_UNIFORM_VEC3);
    set("roomHeight", &roomHeight, SHADER_UNIFORM_FLOAT);
    set("fogColor", &black, SHADER_UNIFORM_VEC3);
    set("viewDirection", &down, SHADER_UNIFORM_VEC3);
    set("emission", &emission, SHADER_UNIFORM_FLOAT);
    set("mazeSize", &size, SHADER_UNIFORM_INT);
    set("cellSize", &cellSize, SHADER_UNIFORM_FLOAT);
    RenderTexture2D canvas = LoadRenderTexture(96, 96);
    for (int barrierCol : {5, 31, 32, 47}) {
        const Vector3 point{MazeGeometry::cellCenter(barrierCol - 1) + .6f, 0,
                            MazeGeometry::cellCenter(3)};
        const Camera3D camera{{point.x, 3, point.z}, point, {0, 0, -1}, .4f, CAMERA_ORTHOGRAPHIC};
        const Vector4 source{MazeGeometry::cellCenter(barrierCol + 1) - .6f, .8f, point.z, 6};
        const Vector4 direction{0, -1, 0, 4}, extent{.2f, 0, 0, .1f};
        set("eyePosition", &camera.position, SHADER_UNIFORM_VEC3);
        set("lightPositions", &source, SHADER_UNIFORM_VEC4);
        set("lightDirections", &direction, SHADER_UNIFORM_VEC4);
        set("lightExtents", &extent, SHADER_UNIFORM_VEC4);
        auto sample = [&](bool enabled, bool barrier) {
            int rows[MazeShaders::MaxMazeRows * 2]{};
            if (barrier)
                rows[3 * 2 + barrierCol / 32] =
                    std::bit_cast<int>(std::uint32_t{1} << (barrierCol % 32));
            SetShaderValueV(shader, GetShaderLocation(shader, "wallRows"), rows,
                            SHADER_UNIFORM_IVEC2, MazeShaders::MaxMazeRows);
            const int count = enabled ? 1 : 0;
            set("lightCount", &count, SHADER_UNIFORM_INT);
            BeginDrawing();
            BeginTextureMode(canvas);
            ClearBackground(BLACK);
            BeginMode3D(camera);
            BeginShaderMode(shader);
            DrawPlane(point, {.4f, .4f}, WHITE);
            EndShaderMode();
            EndMode3D();
            EndTextureMode();
            EndDrawing();
            Image image = LoadImageFromTexture(canvas.texture);
            const Color color = GetImageColor(image, 48, 48);
            UnloadImage(image);
            return color;
        };
        const Color ambient = sample(false, false), lit = sample(true, false);
        const Color blocked = sample(true, true);
        require(lit.r > ambient.r + 15, "a nearby lamp must visibly illuminate the room");
        require(lit.r - ambient.r > lit.b - ambient.b,
                "a warm lamp must retain its color when illuminating surfaces");
        require(std::abs(blocked.r - ambient.r) <= 3,
                "soft lamp shadows must not leak through solid maze walls");
    }
    UnloadRenderTexture(canvas);
    UnloadShader(shader);
}

// Reuse one effect across frames and sizes. Only world highlights may glow: dark areas and UI
// drawn after composition must retain their exact colors, with the caller's framebuffer restored.
void checkBloom() {
    const int originalWidth = GetScreenWidth(), originalHeight = GetScreenHeight();
    Bloom bloom;
    for (int width : {640, 800, 640}) {
        SetWindowSize(width, 480);
        BeginDrawing();
        EndDrawing();
        const int height = GetScreenHeight();
        RenderTexture2D canvas = LoadRenderTexture(width, height);
        for (bool light : {true, false}) {
            const Color background{12, 18, 24, 255}, interfaceColor{35, 62, 90, 255};
            BeginDrawing();
            BeginTextureMode(canvas);
            const unsigned int destination = rlGetActiveFramebuffer();
            require(bloom.begin(), "bloom targets or shader are unavailable");
            ClearBackground(background);
            if (light)
                DrawRectangle(width / 2 - 12, height / 2 - 12, 24, 24, {255, 225, 190, 255});
            bloom.end();
            require(rlGetActiveFramebuffer() == destination,
                    "bloom must restore the caller's framebuffer");
            DrawRectangle(10, 10, 24, 24, interfaceColor);
            EndTextureMode();
            EndDrawing();
            Image image = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&image);
            const Color halo = GetImageColor(image, width / 2 + 16, height / 2);
            const Color dark = GetImageColor(image, width - 2, height - 2);
            const Color ui = GetImageColor(image, 12, 12);
            UnloadImage(image);
            require(light ? halo.r > background.r + 2 : halo.r == background.r,
                    "bloom must soften bright fixtures without leaving trails in later frames");
            require(dark.r == background.r && dark.g == background.g && dark.b == background.b,
                    "bloom must preserve unlit areas after resizing");
            require(ui.r == interfaceColor.r && ui.g == interfaceColor.g &&
                        ui.b == interfaceColor.b,
                    "UI drawn after bloom must stay sharp and keep its exact colors");
        }
        UnloadRenderTexture(canvas);
    }
    SetWindowSize(originalWidth, originalHeight);
    BeginDrawing();
    EndDrawing();
}
}

// Called inside the UI suite's hidden graphics context; gameplay and saved data are untouched.
void runLightingTests() {
    checkWallShadows();
    checkBloom();
}
