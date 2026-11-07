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

// Enough transformed primitives to force automatic batch submission must not move the shader's
// world positions. This guards against black trim when a detailed corridor fills rlgl's buffer.
void checkBatchTransforms() {
    Shader shader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    require(shader.id && shader.id != rlGetShaderIdDefault(), "batch shader did not compile");
    const Vector3 black{}, eye{40, .5f, 4};
    const float emission = 0, roomHeight = MazeGeometry::CeilingHeight;
    const int lightCount = 0;
    SetShaderValue(shader, GetShaderLocation(shader, "eyePosition"), &eye, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fogColor"), &black, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "emission"), &emission, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "roomHeight"), &roomHeight,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, GetShaderLocation(shader, "lightCount"), &lightCount,
                   SHADER_UNIFORM_INT);
    const Camera3D camera{eye, {40, .5f, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
    RenderTexture2D canvas = LoadRenderTexture(96, 96);
    auto sample = [&](int fillers) {
        BeginDrawing();
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        BeginShaderMode(shader);
        DrawCube({40, .5f, 0}, 1, 1, 1, {220, 190, 160, 255});
        for (int index = 0; index < fillers; ++index)
            DrawCube({80, .5f, 0}, .1f, .1f, .1f, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(canvas.texture);
        const Color color = GetImageColor(image, 48, 48);
        UnloadImage(image);
        return color;
    };
    const Color single = sample(0), fullBatch = sample(1500);
    UnloadRenderTexture(canvas);
    UnloadShader(shader);
    require(single.r > 30 && std::abs(single.r - fullBatch.r) <= 1 &&
                std::abs(single.g - fullBatch.g) <= 1 && std::abs(single.b - fullBatch.b) <= 1,
            "full draw batches must retain correct world-space lighting on raised trim");
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

// Float targets must preserve distinct lamp intensities above one until display conversion. A
// byte target silently clips both inputs to the same white and fails this observable comparison.
void checkHdrComposition() {
    constexpr const char* source = R"glsl(#version 330
uniform vec3 radiance;
out vec4 finalColor;
void main() { finalColor = vec4(radiance, 1.0); }
)glsl";
    Shader lamp = LoadShaderFromMemory(nullptr, source);
    require(lamp.id && lamp.id != rlGetShaderIdDefault(), "HDR fixture shader did not compile");
    const int location = GetShaderLocation(lamp, "radiance");
    const int width = GetScreenWidth(), height = GetScreenHeight();
    RenderTexture2D canvas = LoadRenderTexture(width, height);
    {
        Bloom bloom;
        Color results[3]{};
        const float levels[] = {1.0f, 3.0f, 1.0f};
        const Color interfaceColor{35, 62, 90, 255};
        for (int frame = 0; frame < 3; ++frame) {
            const Vector3 radiance{levels[frame], levels[frame] * .70f, levels[frame] * .40f};
            BeginDrawing();
            BeginTextureMode(canvas);
            const unsigned int destination = rlGetActiveFramebuffer();
            require(bloom.begin(true), "HDR targets or compositor are unavailable");
            ClearBackground(BLACK);
            BeginShaderMode(lamp);
            SetShaderValue(lamp, location, &radiance, SHADER_UNIFORM_VEC3);
            DrawRectangle(width / 2 - 12, height / 2 - 12, 24, 24, WHITE);
            EndShaderMode();
            bloom.end();
            require(rlGetActiveFramebuffer() == destination,
                    "HDR composition lost its destination");
            DrawRectangle(10, 10, 24, 24, interfaceColor);
            EndTextureMode();
            EndDrawing();
            Image image = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&image);
            results[frame] = GetImageColor(image, width / 2, height / 2);
            const Color ui = GetImageColor(image, 12, 12);
            const Color dark = GetImageColor(image, width - 2, height - 2);
            UnloadImage(image);
            require(ui.r == interfaceColor.r && ui.g == interfaceColor.g &&
                        ui.b == interfaceColor.b,
                    "HDR tone mapping must leave UI colors unchanged");
            require(dark.r == 0 && dark.g == 0 && dark.b == 0,
                    "HDR glow must not wash out unlit corners");
        }
        require(results[1].r > results[0].r + 5 && results[1].r < 255,
                "HDR must preserve brightness differences above one without clipping to white");
        require(results[1].r > results[1].b + 3,
                "bright warm fixtures must retain a warm highlight shoulder");
        require(std::abs(results[0].r - results[2].r) <= 1,
                "HDR composition must not accumulate bloom between frames");
    }
    UnloadRenderTexture(canvas);
    UnloadShader(lamp);
}

// A polished dielectric must have a stronger aligned source reflection than a rough surface,
// and the highlight must respond to a moving camera instead of being painted into the material.
void checkMaterialHighlights() {
    Shader shader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    require(shader.id && shader.id != rlGetShaderIdDefault(), "material shader did not compile");
    auto set = [&](const char* name, const void* value, int type) {
        const int location = GetShaderLocation(shader, name);
        require(location >= 0, "material fixture uniform is unavailable");
        SetShaderValue(shader, location, value, type);
    };
    const int size = 15, count = 1;
    const float cell = MazeGeometry::CellSize, height = MazeGeometry::CeilingHeight;
    const Vector3 white{1, 1, 1}, black{}, down{0, -1, 0};
    const Vector3 point{9, 0, 9};
    const Vector4 lamp{9, .8f, 7, 6}, facing{0, -1, 0, 1.5f}, extent{.08f, 0, 0, .08f};
    set("fixtureColor", &white, SHADER_UNIFORM_VEC3);
    set("bounceColor", &white, SHADER_UNIFORM_VEC3);
    set("fogColor", &black, SHADER_UNIFORM_VEC3);
    set("viewDirection", &down, SHADER_UNIFORM_VEC3);
    set("mazeSize", &size, SHADER_UNIFORM_INT);
    set("cellSize", &cell, SHADER_UNIFORM_FLOAT);
    set("roomHeight", &height, SHADER_UNIFORM_FLOAT);
    set("lightCount", &count, SHADER_UNIFORM_INT);
    set("lightPositions", &lamp, SHADER_UNIFORM_VEC4);
    set("lightDirections", &facing, SHADER_UNIFORM_VEC4);
    set("lightExtents", &extent, SHADER_UNIFORM_VEC4);
    RenderTexture2D canvas = LoadRenderTexture(96, 96);
    auto sample = [&](float roughness, float across) {
        const Vector4 material{roughness, 0, 0, 0};
        const Camera3D camera{{9 + across, .8f, 11}, point, {0, 1, 0}, 35, CAMERA_PERSPECTIVE};
        set("eyePosition", &camera.position, SHADER_UNIFORM_VEC3);
        set("materialParams", &material, SHADER_UNIFORM_VEC4);
        BeginDrawing();
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        BeginMode3D(camera);
        BeginShaderMode(shader);
        DrawPlane(point, {.8f, .8f}, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        EndDrawing();
        Image image = LoadImageFromTexture(canvas.texture);
        const Color pixel = GetImageColor(image, 48, 48);
        UnloadImage(image);
        return pixel;
    };
    const Color polished = sample(.24f, 0), rough = sample(.95f, 0), moved = sample(.24f, 1.3f);
    UnloadRenderTexture(canvas);
    UnloadShader(shader);
    require(polished.r > rough.r + 12, "polished stone must have a distinct fixture highlight");
    require(polished.r > moved.r + 12, "material highlights must move with the viewing direction");
}
}

// Called inside the UI suite's hidden graphics context; gameplay and saved data are untouched.
void runLightingTests() {
    checkWallShadows();
    checkBatchTransforms();
    checkBloom();
    checkHdrComposition();
    checkMaterialHighlights();
}
