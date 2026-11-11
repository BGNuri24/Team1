#pragma once
#include "../logic/Game.h"
#include "Bloom.h"
#include "HudRenderer.h"
#include "Textures.h"
#include "raylib.h"

class Renderer {
public:
    // Initialize the perspective camera and custom lighting shader, caching its uniform locations
    // when compilation succeeds. Keep valid fallback state when a custom shader is unavailable;
    // material preparation is coordinated separately.
    Renderer();
    // Release the renderer's owned lighting shader while member resource owners clean up their
    // textures and post-processing targets. Application destroys this object before closing the
    // graphics context.
    ~Renderer();
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Renderer(const Renderer&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Renderer& operator=(const Renderer&) = delete;

    // Render the world first, then draw the gameplay HUD in screen space with optional map-peek
    // state. Paused and result views can use drawWorld when gameplay overlays should be omitted.
    void Draw(const Game& CurrentGame, const MapPeek* CurrentMapPeek = nullptr);
    // Build the camera from the game's displayed position, direction, and up vector, then draw
    // lit maze surfaces, details, hazards, and exit. Use post-processing when available and keep
    // the fallback world render functional.
    void DrawWorld(const Game& CurrentGame);
    // Reuse already prepared materials when the requested theme is unchanged. Build a complete
    // replacement first and publish it only when valid, preserving the old resources if material
    // creation fails.
    void PrepareMazeTheme(MazeTheme Theme);

private:
    // Batch nearby floor, ceiling, and exposed wall surfaces using their textures, followed by
    // shallow architectural pieces. Manage texture and culling state explicitly so each surface
    // uses the intended material.
    void DrawMaze(const Game& CurrentGame);
    // Draw the paired opal-glass sconce cups and other fixture geometry with their emissive
    // treatment. Placement stays clear of ceiling-walking eye positions and never changes
    // gameplay collision.
    void DrawLights(const Game& CurrentGame);
    // Layer skirting, cornices, chair rails, stone pieces, and panel frames over the base wall
    // material. Nearby visibility limits the fine geometry while distant texture detail preserves
    // the room's appearance.
    void DrawDetails(const Game& CurrentGame);
    // Draw the run's lethal barriers at the same world-space bounds used by collision, adding
    // their visible hazard treatment. The glow flag selects emissive contribution without
    // creating or moving obstacles.
    void DrawObstacles(const Game& CurrentGame, bool Glow);
    // Draw the final corridor's exit architecture and its lit details using the run's theme and
    // geometry. The glow flag selects its emissive contribution while the logical exit remains a
    // maze cell.
    void DrawExit(const Game& CurrentGame, bool Glow);
    // Upload wall occupancy masks when the run layout changes, then select a bounded list of
    // nearby ceiling and wall sources. Cache layout identity by size and seed so per-frame
    // lighting cost stays bounded.
    void UpdateLights(const Game& CurrentGame);
    // Flush queued vertices before changing the material uniforms for following geometry.
    // Roughness, metalness, relief, clear coat, and wall-surface flags must not retroactively
    // affect a previous batch.
    void GetMaterial(float CurrentRoughness, float Metalness, float Relief, float ClearCoat = 0,
                  bool WallSurface = false);

    Camera3D Cam;
    Textures Tex;
    HudRenderer Hud;
    Bloom CurrentBloom;
    MazeTheme ActiveTheme = ActiveMazeTheme;
    MazePalette CurrentPalette = GetMazePalette(ActiveMazeTheme);
    bool TexturesReady = false;
    Shader Atmosphere{};
    int EyeLocation = -1;
    int DirectionLocation = -1;
    int EmissionLocation = -1;
    int MaterialLocation = -1, HdrLocation = -1, WallSurfaceLocation = -1;
    int LightPositionsLocation = -1;
    int LightDirectionsLocation = -1;
    int LightExtentsLocation = -1;
    int LightCountLocation = -1;
    int WallRowsLocation = -1;
    int MazeSizeLocation = -1;
    int CellSizeLocation = -1;
    int LightMazeSize = 0;
    std::uint32_t LightMazeSeed = 0;
    int FixtureColorLocation = -1;
    int FogColorLocation = -1;
    int BounceColorLocation = -1;
    int RoomHeightLocation = -1;
};
