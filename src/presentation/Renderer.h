#pragma once
#include "../logic/Game.h"
#include "HudRenderer.h"
#include "Textures.h"
#include "raylib.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(const Game& game);
    // Prepare one palette's materials at run creation, keeping them stable until another run is
    // loaded.
    void prepareMazeTheme(MazeTheme theme);

private:
    void drawMaze(const Game& game);
    void drawLights(const Game& game);
    void drawDetails(const Game& game);
    void drawObstacles(const Game& game, bool glow);
    void drawExit(const Game& game, bool glow);
    void updateCeilingLights(const Game& game);

    Camera3D cam;
    Textures tex;
    HudRenderer hud;
    MazeTheme activeTheme = MazeTheme::Slate;
    MazePalette palette = mazePalette(MazeTheme::Slate);
    bool texturesReady = false;
    Shader atmosphere{};
    int eyeLocation = -1;
    int directionLocation = -1;
    int emissionLocation = -1;
    int ceilingLightsLocation = -1;
    int ceilingLightCountLocation = -1;
    int fixtureColorLocation = -1;
    int fogColorLocation = -1;
};
