#pragma once
#include "../logic/Game.h"
#include "../logic/MapPeek.h"
#include "MazePalette.h"

// Draw screen-space gameplay information independently of world geometry and GPU material
// ownership.
class HudRenderer {
public:
    // Compose the timer and optional map UI in screen space after world rendering has finished.
    // The caller controls whether gameplay supplies an active peek state or an ordinary
    // hidden-map hint.
    void Draw(const Game& CurrentGame, const MazePalette& CurrentPalette, const MapPeek* CurrentMapPeek = nullptr) const;

private:
    // Draw the centered translucent countdown digits over the world without an opaque panel or
    // compass. Read the Game timer only; paused and finished frames must not advance it.
    void DrawHud(const Game& CurrentGame, const MazePalette& CurrentPalette, bool ShowReticle) const;
    // Draw a centered enlarged peek using only the run's revealed cells, keeping the timer and
    // window edges clear. Showing a larger map never discloses unexplored passages or changes
    // exploration state.
    void DrawMinimap(const Game& CurrentGame, const MazePalette& CurrentPalette) const;
};
