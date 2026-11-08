#pragma once
#include "../logic/Game.h"
#include "Bloom.h"
#include "HudRenderer.h"
#include "Textures.h"
#include "raylib.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Supply peek state only for active gameplay; pause and result backdrops never reveal the map.
    void draw(const Game& game, const MapPeek* mapPeek = nullptr);
    // Result screens retain the final corridor without carrying the gameplay HUD into the report.
    void drawWorld(const Game& game);
    // Prepare one palette's materials at run creation, keeping them stable until another run is
    // loaded.
    void prepareMazeTheme(MazeTheme theme);

private:
    void drawMaze(const Game& game);
    void drawLights(const Game& game);
    void drawDetails(const Game& game);
    void drawObstacles(const Game& game, bool glow);
    void drawExit(const Game& game, bool glow);
    void updateLights(const Game& game);
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
