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
private:
    void drawMaze(const Game& game);
    void drawMinimap(const Game& game);

    Camera3D cam;
    Textures tex;
};