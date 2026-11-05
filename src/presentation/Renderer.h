#pragma once
#include "raylib.h"
#include "../logic/Game.h"
#include "Textures.h"

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(const Game& game);
    // Prepare one palette's materials at run creation, keeping them stable until another run is loaded.
    void prepareMazeTheme(MazeTheme theme);
private:
    void drawMaze(const Game& game);
    void drawMinimap(const Game& game);
    void drawHud(const Game& game);
    void drawLights(const Game& game);
    void drawDetails(const Game& game);
    void updateCeilingLights(const Game& game);

    Camera3D cam;
    Textures tex;
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
