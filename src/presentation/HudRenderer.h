#pragma once
#include "../logic/Game.h"
#include "../logic/MapPeek.h"
#include "MazePalette.h"

// Draw screen-space gameplay information independently of world geometry and GPU material
// ownership.
class HudRenderer {
public:
    void draw(const Game& game, const MazePalette& palette, const MapPeek* mapPeek = nullptr) const;

private:
    void drawHud(const Game& game, const MazePalette& palette, bool showReticle) const;
    void drawMinimap(const Game& game, const MazePalette& palette) const;
};
