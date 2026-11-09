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
// Throw a descriptive runtime_error when a GPU-output expectation fails. The caller's UI-suite
// graphics context remains responsible for rendering setup and cleanup.
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Read actual shader output on the GPU, including wall columns on both sides of the 32-bit split.
// The same floor must be visibly lit in an open passage and stay at ambient behind a solid wall.
void checkWallShadows() {
    Shader shader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    require(shader.id && shader.id != rlGetShaderIdDefault(), "lamp shader did not compile");
    // Find a named lighting uniform and upload the supplied test value with its declared shader
    // type. This keeps fixture setup concise while exercising the production shader.
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
        // Render a controlled floor sample with optional lighting and an optional wall barrier,
        // then read its pixel color. Comparing these cases verifies actual GPU shadow visibility
        // rather than duplicating shader formulas.
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
    // Render the same probe after a chosen number of filler primitives force rlgl batch
    // submission. The sampled result detects accidental repeated model transforms across batch
    // boundaries.
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
// Emit the supplied HDR radiance directly into the test target. Known values above one let the
// test detect premature clipping before final display conversion.
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

// A polished dielectric must have a stronger aligned source reflection than a rough surface, and
// the highlight must respond to a moving camera instead of being painted into the material.
void checkMaterialHighlights() {
    Shader shader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    require(shader.id && shader.id != rlGetShaderIdDefault(), "material shader did not compile");
    // Upload one named material-lighting uniform for the controlled highlight fixture. Camera and
    // roughness variants then exercise the same compiled production shader.
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
    // Render and sample a material at the requested roughness and lateral camera position.
    // Comparing outputs checks that polished highlights are stronger and respond to viewpoint
    // changes.
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

// A red object must appear in polished floor reflections, while rough flooring stays diffuse.
// Exercise the first frame, target resizing, repeated frames and UI after the actual MRT pass.
void checkSurfaceEffects() {
    constexpr const char* source = R"glsl(#version 330
in vec3 worldPosition;
in vec4 fragColor;
uniform vec3 eyePosition;
uniform float roughness;
layout(location = 0) out vec4 finalColor;
layout(location = 1) out vec4 surfaceData;
// Write controlled normal, roughness, and reflectance data plus a known scene color for
// surface-effect tests. This isolates post-processing behavior from the production lighting
// model.
void main() {
    vec3 n = normalize(cross(dFdx(worldPosition), dFdy(worldPosition)));
    if (dot(n, eyePosition - worldPosition) < 0.0) n = -n;
    n /= abs(n.x) + abs(n.y) + abs(n.z);
    vec2 e = n.xy;
    if (n.z < 0.0) e = (1.0 - abs(e.yx)) * sign(e + 0.000001);
    surfaceData = vec4(e * 0.5 + 0.5, roughness, 0.20);
    finalColor = vec4(fragColor.rgb * 0.4, 1.0);
}
)glsl";
    Shader fixture = LoadShaderFromMemory(MazeShaders::Vertex, source);
    require(fixture.id && fixture.id != rlGetShaderIdDefault(),
            "surface reflection fixture did not compile");
    const int eyeLocation = GetShaderLocation(fixture, "eyePosition");
    const int roughnessLocation = GetShaderLocation(fixture, "roughness");
    const int originalWidth = GetScreenWidth(), originalHeight = GetScreenHeight();
    const Camera3D camera{{0, 1.2f, 3}, {0, .6f, 0}, {0, 1, 0}, 55, CAMERA_PERSPECTIVE};
    const Color interfaceColor{35, 62, 90, 255};
    {
        Bloom bloom;
        for (int width : {640, 800, 640}) {
            SetWindowSize(width, 480);
            BeginDrawing();
            EndDrawing();
            RenderTexture2D canvas = LoadRenderTexture(width, 480);
            // Render the controlled reflection fixture at the requested roughness with surface
            // effects enabled or bypassed. Compare GPU pixels to isolate reflected scene color
            // from ordinary direct rendering.
            auto render = [&](float roughness, bool effects) {
                BeginDrawing();
                BeginTextureMode(canvas);
                const unsigned int destination = rlGetActiveFramebuffer();
                require(bloom.begin(true), "surface targets are unavailable");
                ClearBackground(BLACK);
                BeginMode3D(camera);
                if (effects)
                    bloom.setView(rlGetMatrixModelview(), rlGetMatrixProjection());
                BeginShaderMode(fixture);
                SetShaderValue(fixture, eyeLocation, &camera.position, SHADER_UNIFORM_VEC3);
                SetShaderValue(fixture, roughnessLocation, &roughness, SHADER_UNIFORM_FLOAT);
                DrawPlane({0, 0, 0}, {6, 6}, {150, 150, 150, 255});
                DrawCube({0, .5f, 0}, 1, 1, 1, {230, 25, 15, 255});
                EndShaderMode();
                EndMode3D();
                bloom.end();
                require(rlGetActiveFramebuffer() == destination,
                        "surface effects must restore the caller's framebuffer");
                DrawRectangle(10, 10, 24, 24, interfaceColor);
                DrawText("12:34", width / 2 - 45, 24, 32, WHITE);
                EndTextureMode();
                EndDrawing();
                Image result = LoadImageFromTexture(canvas.texture);
                ImageFlipVertical(&result);
                const Color ui = GetImageColor(result, 12, 12);
                require(ui.r == interfaceColor.r && ui.g == interfaceColor.g &&
                            ui.b == interfaceColor.b,
                        "surface lighting must leave the UI's colors and placement intact");
                int textPixels = 0;
                for (int y = 24; y < 56; ++y)
                    for (int x = width / 2 - 45; x < width / 2 + 65; ++x) {
                        const Color pixel = GetImageColor(result, x, y);
                        textPixels += pixel.r == 255 && pixel.g == 255 && pixel.b == 255;
                    }
                require(textPixels > 100,
                        "the timer must remain visible after surface composition");
                return result;
            };
            Image baseline = render(.20f, false);
            Image polished = render(.20f, true);
            Image rough = render(.96f, true);
            Image repeated = render(.20f, true);
            const Vector2 mirrored = GetWorldToScreen({0, -.5f, 0}, camera);
            const Color reflection = GetImageColor(polished, (int)mirrored.x, (int)mirrored.y);
            const Color diffuse = GetImageColor(rough, (int)mirrored.x, (int)mirrored.y);
            const Color stable = GetImageColor(repeated, (int)mirrored.x, (int)mirrored.y);
            require(reflection.r > diffuse.r + 5 && reflection.r > reflection.g + 5,
                    "polished surfaces must reflect actual colored scene geometry");
            require(std::abs(reflection.r - stable.r) <= 1,
                    "repeated surface passes must not accumulate stale reflections");
            int darkened = 0;
            for (int y = 100; y < 440; ++y)
                for (int x = width / 2 - 150; x < width / 2 + 150; ++x) {
                    const Color before = GetImageColor(baseline, x, y);
                    const Color after = GetImageColor(rough, x, y);
                    darkened += before.r > after.r + 3 && before.g > after.g + 3;
                }
            require(darkened > 30, "nearby geometry must produce visible contact occlusion");
            for (Image image : {baseline, polished, rough, repeated})
                UnloadImage(image);
            UnloadRenderTexture(canvas);
        }
    }
    UnloadShader(fixture);
    SetWindowSize(originalWidth, originalHeight);
    BeginDrawing();
    EndDrawing();
}
} // namespace

// Run lighting, batching, HDR, bloom, material, and surface-effect checks inside the UI suite's
// hidden graphics context. Inspect rendered results without updating gameplay or writing player
// saves.
void runLightingTests() {
    checkWallShadows();
    checkBatchTransforms();
    checkBloom();
    checkHdrComposition();
    checkMaterialHighlights();
    checkSurfaceEffects();
}
