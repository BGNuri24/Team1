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
    void draw(const Game& game, const MapPeek* mapPeek = nullptr);
    // Build the camera from the game's displayed position, direction, and up vector, then draw
    // lit maze surfaces, details, hazards, and exit. Use post-processing when available and keep
    // the fallback world render functional.
    void drawWorld(const Game& game);
    // Reuse already prepared materials when the requested theme is unchanged. Build a complete
    // replacement first and publish it only when valid, preserving the old resources if material
    // creation fails.
    void prepareMazeTheme(MazeTheme theme);

private:
    // Batch nearby floor, ceiling, and exposed wall surfaces using their textures, followed by
    // shallow architectural pieces. Manage texture and culling state explicitly so each surface
    // uses the intended material.
    void drawMaze(const Game& game);
    // Draw the paired opal-glass sconce cups and other fixture geometry with their emissive
    // treatment. Placement stays clear of ceiling-walking eye positions and never changes
    // gameplay collision.
    void drawLights(const Game& game);
    // Layer skirting, cornices, chair rails, stone pieces, and panel frames over the base wall
    // material. Nearby visibility limits the fine geometry while distant texture detail preserves
    // the room's appearance.
    void drawDetails(const Game& game);
    // Draw the run's lethal barriers at the same world-space bounds used by collision, adding
    // their visible hazard treatment. The glow flag selects emissive contribution without
    // creating or moving obstacles.
    void drawObstacles(const Game& game, bool glow);
    // Draw the final corridor's exit architecture and its lit details using the run's theme and
    // geometry. The glow flag selects its emissive contribution while the logical exit remains a
    // maze cell.
    void drawExit(const Game& game, bool glow);
    // Upload wall occupancy masks when the run layout changes, then select a bounded list of
    // nearby ceiling and wall sources. Cache layout identity by size and seed so per-frame
    // lighting cost stays bounded.
    void updateLights(const Game& game);
    // Flush queued vertices before changing the material uniforms for following geometry.
    // Roughness, metalness, relief, clear coat, and wall-surface flags must not retroactively
    // affect a previous batch.
    void material(float roughness, float metalness, float relief, float clearCoat = 0,
                  bool wallSurface = false);

    Camera3D cam;
    Textures tex;
    HudRenderer hud;
    Bloom bloom;
    MazeTheme activeTheme = ActiveMazeTheme;
    MazePalette palette = mazePalette(ActiveMazeTheme);
    bool texturesReady = false;
    Shader atmosphere{};
    int eyeLocation = -1;
    int directionLocation = -1;
    int emissionLocation = -1;
    int materialLocation = -1, hdrLocation = -1, wallSurfaceLocation = -1;
    int lightPositionsLocation = -1;
    int lightDirectionsLocation = -1;
    int lightExtentsLocation = -1;
    int lightCountLocation = -1;
    int wallRowsLocation = -1;
    int mazeSizeLocation = -1;
    int cellSizeLocation = -1;
    int lightMazeSize = 0;
    std::uint32_t lightMazeSeed = 0;
    int fixtureColorLocation = -1;
    int fogColorLocation = -1;
    int bounceColorLocation = -1;
    int roomHeightLocation = -1;
};
