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
void Require(bool Condition, const char* Message) {
    if (!Condition)
        throw std::runtime_error(Message);
}

// Read actual shader output on the GPU, including wall columns on both sides of the 32-bit split.
// The same floor must be visibly lit in an open passage and stay at ambient behind a solid wall.
void CheckWallShadows() {
    Shader CurrentShader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    Require(CurrentShader.id && CurrentShader.id != rlGetShaderIdDefault(), "lamp shader did not compile");
    // Find a named lighting uniform and upload the supplied test value with its declared shader
    // type. This keeps fixture setup concise while exercising the production shader.
    auto CurrentSet = [&](const char* CurrentName, const void* Value, int Type) {
        const int Location = GetShaderLocation(CurrentShader, CurrentName);
        Require(Location >= 0, "lamp shader is missing a required uniform");
        SetShaderValue(CurrentShader, Location, Value, Type);
    };
    const int CurrentSize = 50;
    const float CurrentCellSize = MazeGeometry::CellSize, Emission = 0;
    const float RoomHeight = MazeGeometry::CeilingHeight;
    const Vector3 Warm{1, .72f, .40f}, CurrentBlack{}, Down{0, -1, 0};
    CurrentSet("FixtureColor", &Warm, SHADER_UNIFORM_VEC3);
    CurrentSet("BounceColor", &Warm, SHADER_UNIFORM_VEC3);
    CurrentSet("RoomHeight", &RoomHeight, SHADER_UNIFORM_FLOAT);
    CurrentSet("FogColor", &CurrentBlack, SHADER_UNIFORM_VEC3);
    CurrentSet("ViewDirection", &Down, SHADER_UNIFORM_VEC3);
    CurrentSet("Emission", &Emission, SHADER_UNIFORM_FLOAT);
    CurrentSet("MazeSize", &CurrentSize, SHADER_UNIFORM_INT);
    CurrentSet("CellSize", &CurrentCellSize, SHADER_UNIFORM_FLOAT);
    RenderTexture2D Canvas = LoadRenderTexture(96, 96);
    for (int BarrierCol : {5, 31, 32, 47}) {
        const Vector3 Point{MazeGeometry::CellCenter(BarrierCol - 1) + .6f, 0,
                            MazeGeometry::CellCenter(3)};
        const Camera3D CurrentCamera{{Point.x, 3, Point.z}, Point, {0, 0, -1}, .4f, CAMERA_ORTHOGRAPHIC};
        const Vector4 Source{MazeGeometry::CellCenter(BarrierCol + 1) - .6f, .8f, Point.z, 6};
        const Vector4 Direction{0, -1, 0, 4}, Extent{.2f, 0, 0, .1f};
        CurrentSet("EyePosition", &CurrentCamera.position, SHADER_UNIFORM_VEC3);
        CurrentSet("LightPositions", &Source, SHADER_UNIFORM_VEC4);
        CurrentSet("LightDirections", &Direction, SHADER_UNIFORM_VEC4);
        CurrentSet("LightExtents", &Extent, SHADER_UNIFORM_VEC4);
        // Render a controlled floor sample with optional lighting and an optional wall barrier,
        // then read its pixel color. Comparing these cases verifies actual GPU shadow visibility
        // rather than duplicating shader formulas.
        auto CurrentSample = [&](bool Enabled, bool CurrentBarrier) {
            int Rows[MazeShaders::MaxMazeRows * 2]{};
            if (CurrentBarrier)
                Rows[3 * 2 + BarrierCol / 32] =
                    std::bit_cast<int>(std::uint32_t{1} << (BarrierCol % 32));
            SetShaderValueV(CurrentShader, GetShaderLocation(CurrentShader, "WallRows"), Rows,
                            SHADER_UNIFORM_IVEC2, MazeShaders::MaxMazeRows);
            const int CurrentCount = Enabled ? 1 : 0;
            CurrentSet("LightCount", &CurrentCount, SHADER_UNIFORM_INT);
            BeginDrawing();
            BeginTextureMode(Canvas);
            ClearBackground(BLACK);
            BeginMode3D(CurrentCamera);
            BeginShaderMode(CurrentShader);
            DrawPlane(Point, {.4f, .4f}, WHITE);
            EndShaderMode();
            EndMode3D();
            EndTextureMode();
            EndDrawing();
            Image CurrentImage = LoadImageFromTexture(Canvas.texture);
            const Color CurrentColor = GetImageColor(CurrentImage, 48, 48);
            UnloadImage(CurrentImage);
            return CurrentColor;
        };
        const Color Ambient = CurrentSample(false, false), Lit = CurrentSample(true, false);
        const Color Blocked = CurrentSample(true, true);
        Require(Lit.r > Ambient.r + 15, "a nearby lamp must visibly illuminate the room");
        Require(Lit.r - Ambient.r > Lit.b - Ambient.b,
                "a warm lamp must retain its color when illuminating surfaces");
        Require(std::abs(Blocked.r - Ambient.r) <= 3,
                "soft lamp shadows must not leak through solid maze walls");
    }
    UnloadRenderTexture(Canvas);
    UnloadShader(CurrentShader);
}

// Enough transformed primitives to force automatic batch submission must not move the shader's
// world positions. This guards against black trim when a detailed corridor fills rlgl's buffer.
void CheckBatchTransforms() {
    Shader CurrentShader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    Require(CurrentShader.id && CurrentShader.id != rlGetShaderIdDefault(), "batch shader did not compile");
    const Vector3 CurrentBlack{}, Eye{40, .5f, 4};
    const float Emission = 0, RoomHeight = MazeGeometry::CeilingHeight;
    const int LightCount = 0;
    SetShaderValue(CurrentShader, GetShaderLocation(CurrentShader, "EyePosition"), &Eye, SHADER_UNIFORM_VEC3);
    SetShaderValue(CurrentShader, GetShaderLocation(CurrentShader, "FogColor"), &CurrentBlack, SHADER_UNIFORM_VEC3);
    SetShaderValue(CurrentShader, GetShaderLocation(CurrentShader, "Emission"), &Emission, SHADER_UNIFORM_FLOAT);
    SetShaderValue(CurrentShader, GetShaderLocation(CurrentShader, "RoomHeight"), &RoomHeight,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(CurrentShader, GetShaderLocation(CurrentShader, "LightCount"), &LightCount,
                   SHADER_UNIFORM_INT);
    const Camera3D CurrentCamera{Eye, {40, .5f, 0}, {0, 1, 0}, 45, CAMERA_PERSPECTIVE};
    RenderTexture2D Canvas = LoadRenderTexture(96, 96);
    // Render the same probe after a chosen number of filler primitives force rlgl batch
    // submission. The sampled result detects accidental repeated model transforms across batch
    // boundaries.
    auto CurrentSample = [&](int Fillers) {
        BeginDrawing();
        BeginTextureMode(Canvas);
        ClearBackground(BLACK);
        BeginMode3D(CurrentCamera);
        BeginShaderMode(CurrentShader);
        DrawCube({40, .5f, 0}, 1, 1, 1, {220, 190, 160, 255});
        for (int Index = 0; Index < Fillers; ++Index)
            DrawCube({80, .5f, 0}, .1f, .1f, .1f, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        EndDrawing();
        Image CurrentImage = LoadImageFromTexture(Canvas.texture);
        const Color CurrentColor = GetImageColor(CurrentImage, 48, 48);
        UnloadImage(CurrentImage);
        return CurrentColor;
    };
    const Color Single = CurrentSample(0), FullBatch = CurrentSample(1500);
    UnloadRenderTexture(Canvas);
    UnloadShader(CurrentShader);
    Require(Single.r > 30 && std::abs(Single.r - FullBatch.r) <= 1 &&
                std::abs(Single.g - FullBatch.g) <= 1 && std::abs(Single.b - FullBatch.b) <= 1,
            "full draw batches must retain correct world-space lighting on raised trim");
}

// Reuse one effect across frames and sizes. Only world highlights may glow: dark areas and UI
// drawn after composition must retain their exact colors, with the caller's framebuffer restored.
void CheckBloom() {
    const int OriginalWidth = GetScreenWidth(), OriginalHeight = GetScreenHeight();
    Bloom CurrentBloom;
    for (int Width : {640, 800, 640}) {
        SetWindowSize(Width, 480);
        BeginDrawing();
        EndDrawing();
        const int Height = GetScreenHeight();
        RenderTexture2D Canvas = LoadRenderTexture(Width, Height);
        for (bool Light : {true, false}) {
            const Color Background{12, 18, 24, 255}, InterfaceColor{35, 62, 90, 255};
            BeginDrawing();
            BeginTextureMode(Canvas);
            const unsigned int Destination = rlGetActiveFramebuffer();
            Require(CurrentBloom.Begin(), "bloom targets or shader are unavailable");
            ClearBackground(Background);
            if (Light)
                DrawRectangle(Width / 2 - 12, Height / 2 - 12, 24, 24, {255, 225, 190, 255});
            CurrentBloom.End();
            Require(rlGetActiveFramebuffer() == Destination,
                    "bloom must restore the caller's framebuffer");
            DrawRectangle(10, 10, 24, 24, InterfaceColor);
            EndTextureMode();
            EndDrawing();
            Image CurrentImage = LoadImageFromTexture(Canvas.texture);
            ImageFlipVertical(&CurrentImage);
            const Color Halo = GetImageColor(CurrentImage, Width / 2 + 16, Height / 2);
            const Color Dark = GetImageColor(CurrentImage, Width - 2, Height - 2);
            const Color CurrentUi = GetImageColor(CurrentImage, 12, 12);
            UnloadImage(CurrentImage);
            Require(Light ? Halo.r > Background.r + 2 : Halo.r == Background.r,
                    "bloom must soften bright fixtures without leaving trails in later frames");
            Require(Dark.r == Background.r && Dark.g == Background.g && Dark.b == Background.b,
                    "bloom must preserve unlit areas after resizing");
            Require(CurrentUi.r == InterfaceColor.r && CurrentUi.g == InterfaceColor.g &&
                        CurrentUi.b == InterfaceColor.b,
                    "UI drawn after bloom must stay sharp and keep its exact colors");
        }
        UnloadRenderTexture(Canvas);
    }
    SetWindowSize(OriginalWidth, OriginalHeight);
    BeginDrawing();
    EndDrawing();
}

// Float targets must preserve distinct lamp intensities above one until display conversion. A
// byte target silently clips both inputs to the same white and fails this observable comparison.
void CheckHdrComposition() {
    constexpr const char* Source = R"glsl(#version 330
uniform vec3 Radiance;
out vec4 FinalColor;
// Emit the supplied HDR radiance directly into the test target. Known values above one let the
// test detect premature clipping before final display conversion.
void main() { FinalColor = vec4(Radiance, 1.0); }
)glsl";
    Shader Lamp = LoadShaderFromMemory(nullptr, Source);
    Require(Lamp.id && Lamp.id != rlGetShaderIdDefault(), "HDR fixture shader did not compile");
    const int Location = GetShaderLocation(Lamp, "Radiance");
    const int Width = GetScreenWidth(), Height = GetScreenHeight();
    RenderTexture2D Canvas = LoadRenderTexture(Width, Height);
    {
        Bloom CurrentBloom;
        Color Results[3]{};
        const float CurrentLevels[] = {1.0f, 3.0f, 1.0f};
        const Color InterfaceColor{35, 62, 90, 255};
        for (int Frame = 0; Frame < 3; ++Frame) {
            const Vector3 Radiance{CurrentLevels[Frame], CurrentLevels[Frame] * .70f, CurrentLevels[Frame] * .40f};
            BeginDrawing();
            BeginTextureMode(Canvas);
            const unsigned int Destination = rlGetActiveFramebuffer();
            Require(CurrentBloom.Begin(true), "HDR targets or compositor are unavailable");
            ClearBackground(BLACK);
            BeginShaderMode(Lamp);
            SetShaderValue(Lamp, Location, &Radiance, SHADER_UNIFORM_VEC3);
            DrawRectangle(Width / 2 - 12, Height / 2 - 12, 24, 24, WHITE);
            EndShaderMode();
            CurrentBloom.End();
            Require(rlGetActiveFramebuffer() == Destination,
                    "HDR composition lost its destination");
            DrawRectangle(10, 10, 24, 24, InterfaceColor);
            EndTextureMode();
            EndDrawing();
            Image CurrentImage = LoadImageFromTexture(Canvas.texture);
            ImageFlipVertical(&CurrentImage);
            Results[Frame] = GetImageColor(CurrentImage, Width / 2, Height / 2);
            const Color CurrentUi = GetImageColor(CurrentImage, 12, 12);
            const Color Dark = GetImageColor(CurrentImage, Width - 2, Height - 2);
            UnloadImage(CurrentImage);
            Require(CurrentUi.r == InterfaceColor.r && CurrentUi.g == InterfaceColor.g &&
                        CurrentUi.b == InterfaceColor.b,
                    "HDR tone mapping must leave UI colors unchanged");
            Require(Dark.r == 0 && Dark.g == 0 && Dark.b == 0,
                    "HDR glow must not wash out unlit corners");
        }
        Require(Results[1].r > Results[0].r + 5 && Results[1].r < 255,
                "HDR must preserve brightness differences above one without clipping to white");
        Require(Results[1].r > Results[1].b + 3,
                "bright warm fixtures must retain a warm highlight shoulder");
        Require(std::abs(Results[0].r - Results[2].r) <= 1,
                "HDR composition must not accumulate bloom between frames");
    }
    UnloadRenderTexture(Canvas);
    UnloadShader(Lamp);
}

// A polished dielectric must have a stronger aligned source reflection than a rough surface, and
// the highlight must respond to a moving camera instead of being painted into the material.
void CheckMaterialHighlights() {
    Shader CurrentShader = LoadShaderFromMemory(MazeShaders::Vertex, MazeShaders::Fragment);
    Require(CurrentShader.id && CurrentShader.id != rlGetShaderIdDefault(), "material shader did not compile");
    // Upload one named material-lighting uniform for the controlled highlight fixture. Camera and
    // roughness variants then exercise the same compiled production shader.
    auto CurrentSet = [&](const char* CurrentName, const void* Value, int Type) {
        const int Location = GetShaderLocation(CurrentShader, CurrentName);
        Require(Location >= 0, "material fixture uniform is unavailable");
        SetShaderValue(CurrentShader, Location, Value, Type);
    };
    const int CurrentSize = 15, CurrentCount = 1;
    const float CurrentCell = MazeGeometry::CellSize, Height = MazeGeometry::CeilingHeight;
    const Vector3 White{1, 1, 1}, CurrentBlack{}, Down{0, -1, 0};
    const Vector3 Point{9, 0, 9};
    const Vector4 Lamp{9, .8f, 7, 6}, Facing{0, -1, 0, 1.5f}, Extent{.08f, 0, 0, .08f};
    CurrentSet("FixtureColor", &White, SHADER_UNIFORM_VEC3);
    CurrentSet("BounceColor", &White, SHADER_UNIFORM_VEC3);
    CurrentSet("FogColor", &CurrentBlack, SHADER_UNIFORM_VEC3);
    CurrentSet("ViewDirection", &Down, SHADER_UNIFORM_VEC3);
    CurrentSet("MazeSize", &CurrentSize, SHADER_UNIFORM_INT);
    CurrentSet("CellSize", &CurrentCell, SHADER_UNIFORM_FLOAT);
    CurrentSet("RoomHeight", &Height, SHADER_UNIFORM_FLOAT);
    CurrentSet("LightCount", &CurrentCount, SHADER_UNIFORM_INT);
    CurrentSet("LightPositions", &Lamp, SHADER_UNIFORM_VEC4);
    CurrentSet("LightDirections", &Facing, SHADER_UNIFORM_VEC4);
    CurrentSet("LightExtents", &Extent, SHADER_UNIFORM_VEC4);
    RenderTexture2D Canvas = LoadRenderTexture(96, 96);
    // Render and sample a material at the requested roughness and lateral camera position.
    // Comparing outputs checks that polished highlights are stronger and respond to viewpoint
    // changes.
    auto CurrentSample = [&](float CurrentRoughness, float Across) {
        const Vector4 Material{CurrentRoughness, 0, 0, 0};
        const Camera3D CurrentCamera{{9 + Across, .8f, 11}, Point, {0, 1, 0}, 35, CAMERA_PERSPECTIVE};
        CurrentSet("EyePosition", &CurrentCamera.position, SHADER_UNIFORM_VEC3);
        CurrentSet("MaterialParams", &Material, SHADER_UNIFORM_VEC4);
        BeginDrawing();
        BeginTextureMode(Canvas);
        ClearBackground(BLACK);
        BeginMode3D(CurrentCamera);
        BeginShaderMode(CurrentShader);
        DrawPlane(Point, {.8f, .8f}, WHITE);
        EndShaderMode();
        EndMode3D();
        EndTextureMode();
        EndDrawing();
        Image CurrentImage = LoadImageFromTexture(Canvas.texture);
        const Color Pixel = GetImageColor(CurrentImage, 48, 48);
        UnloadImage(CurrentImage);
        return Pixel;
    };
    const Color Polished = CurrentSample(.24f, 0), Rough = CurrentSample(.95f, 0), Moved = CurrentSample(.24f, 1.3f);
    UnloadRenderTexture(Canvas);
    UnloadShader(CurrentShader);
    Require(Polished.r > Rough.r + 12, "polished stone must have a distinct fixture highlight");
    Require(Polished.r > Moved.r + 12, "material highlights must move with the viewing direction");
}

// A red object must appear in polished floor reflections, while rough flooring stays diffuse.
// Exercise the first frame, target resizing, repeated frames and UI after the actual MRT pass.
void CheckSurfaceEffects() {
    constexpr const char* Source = R"glsl(#version 330
in vec3 WorldPosition;
in vec4 fragColor;
uniform vec3 EyePosition;
uniform float Roughness;
layout(location = 0) out vec4 FinalColor;
layout(location = 1) out vec4 SurfaceData;
// Write controlled normal, roughness, and reflectance data plus a known scene color for
// surface-effect tests. This isolates post-processing behavior from the production lighting
// model.
void main() {
    vec3 N = normalize(cross(dFdx(WorldPosition), dFdy(WorldPosition)));
    if (dot(N, EyePosition - WorldPosition) < 0.0) N = -N;
    N /= abs(N.x) + abs(N.y) + abs(N.z);
    vec2 E = N.xy;
    if (N.z < 0.0) E = (1.0 - abs(E.yx)) * sign(E + 0.000001);
    SurfaceData = vec4(E * 0.5 + 0.5, Roughness, 0.20);
    FinalColor = vec4(fragColor.rgb * 0.4, 1.0);
}
)glsl";
    Shader CurrentFixture = LoadShaderFromMemory(MazeShaders::Vertex, Source);
    Require(CurrentFixture.id && CurrentFixture.id != rlGetShaderIdDefault(),
            "surface reflection fixture did not compile");
    const int EyeLocation = GetShaderLocation(CurrentFixture, "EyePosition");
    const int RoughnessLocation = GetShaderLocation(CurrentFixture, "Roughness");
    const int OriginalWidth = GetScreenWidth(), OriginalHeight = GetScreenHeight();
    const Camera3D CurrentCamera{{0, 1.2f, 3}, {0, .6f, 0}, {0, 1, 0}, 55, CAMERA_PERSPECTIVE};
    const Color InterfaceColor{35, 62, 90, 255};
    {
        Bloom CurrentBloom;
        for (int Width : {640, 800, 640}) {
            SetWindowSize(Width, 480);
            BeginDrawing();
            EndDrawing();
            RenderTexture2D Canvas = LoadRenderTexture(Width, 480);
            // Render the controlled reflection fixture at the requested roughness with surface
            // effects enabled or bypassed. Compare GPU pixels to isolate reflected scene color
            // from ordinary direct rendering.
            auto CurrentRender = [&](float CurrentRoughness, bool Effects) {
                BeginDrawing();
                BeginTextureMode(Canvas);
                const unsigned int Destination = rlGetActiveFramebuffer();
                Require(CurrentBloom.Begin(true), "surface targets are unavailable");
                ClearBackground(BLACK);
                BeginMode3D(CurrentCamera);
                if (Effects)
                    CurrentBloom.SetView(rlGetMatrixModelview(), rlGetMatrixProjection());
                BeginShaderMode(CurrentFixture);
                SetShaderValue(CurrentFixture, EyeLocation, &CurrentCamera.position, SHADER_UNIFORM_VEC3);
                SetShaderValue(CurrentFixture, RoughnessLocation, &CurrentRoughness, SHADER_UNIFORM_FLOAT);
                DrawPlane({0, 0, 0}, {6, 6}, {150, 150, 150, 255});
                DrawCube({0, .5f, 0}, 1, 1, 1, {230, 25, 15, 255});
                EndShaderMode();
                EndMode3D();
                CurrentBloom.End();
                Require(rlGetActiveFramebuffer() == Destination,
                        "surface effects must restore the caller's framebuffer");
                DrawRectangle(10, 10, 24, 24, InterfaceColor);
                DrawText("12:34", Width / 2 - 45, 24, 32, WHITE);
                EndTextureMode();
                EndDrawing();
                Image Result = LoadImageFromTexture(Canvas.texture);
                ImageFlipVertical(&Result);
                const Color CurrentUi = GetImageColor(Result, 12, 12);
                Require(CurrentUi.r == InterfaceColor.r && CurrentUi.g == InterfaceColor.g &&
                            CurrentUi.b == InterfaceColor.b,
                        "surface lighting must leave the UI's colors and placement intact");
                int TextPixels = 0;
                for (int CurrentY = 24; CurrentY < 56; ++CurrentY)
                    for (int CurrentX = Width / 2 - 45; CurrentX < Width / 2 + 65; ++CurrentX) {
                        const Color Pixel = GetImageColor(Result, CurrentX, CurrentY);
                        TextPixels += Pixel.r == 255 && Pixel.g == 255 && Pixel.b == 255;
                    }
                Require(TextPixels > 100,
                        "the timer must remain visible after surface composition");
                return Result;
            };
            Image Baseline = CurrentRender(.20f, false);
            Image Polished = CurrentRender(.20f, true);
            Image Rough = CurrentRender(.96f, true);
            Image CurrentRepeated = CurrentRender(.20f, true);
            const Vector2 Mirrored = GetWorldToScreen({0, -.5f, 0}, CurrentCamera);
            const Color CurrentReflection = GetImageColor(Polished, (int)Mirrored.x, (int)Mirrored.y);
            const Color Diffuse = GetImageColor(Rough, (int)Mirrored.x, (int)Mirrored.y);
            const Color CurrentStable = GetImageColor(CurrentRepeated, (int)Mirrored.x, (int)Mirrored.y);
            Require(CurrentReflection.r > Diffuse.r + 5 && CurrentReflection.r > CurrentReflection.g + 5,
                    "polished surfaces must reflect actual colored scene geometry");
            Require(std::abs(CurrentReflection.r - CurrentStable.r) <= 1,
                    "repeated surface passes must not accumulate stale reflections");
            int Darkened = 0;
            for (int CurrentY = 100; CurrentY < 440; ++CurrentY)
                for (int CurrentX = Width / 2 - 150; CurrentX < Width / 2 + 150; ++CurrentX) {
                    const Color Before = GetImageColor(Baseline, CurrentX, CurrentY);
                    const Color After = GetImageColor(Rough, CurrentX, CurrentY);
                    Darkened += Before.r > After.r + 3 && Before.g > After.g + 3;
                }
            Require(Darkened > 30, "nearby geometry must produce visible contact occlusion");
            for (Image CurrentImage : {Baseline, Polished, Rough, CurrentRepeated})
                UnloadImage(CurrentImage);
            UnloadRenderTexture(Canvas);
        }
    }
    UnloadShader(CurrentFixture);
    SetWindowSize(OriginalWidth, OriginalHeight);
    BeginDrawing();
    EndDrawing();
}
} // namespace

// Run lighting, batching, HDR, bloom, material, and surface-effect checks inside the UI suite's
// hidden graphics context. Inspect rendered results without updating gameplay or writing player
// saves.
void RunLightingTests() {
    CheckWallShadows();
    CheckBatchTransforms();
    CheckBloom();
    CheckHdrComposition();
    CheckMaterialHighlights();
    CheckSurfaceEffects();
}
