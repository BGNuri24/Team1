#pragma once
#include "MazePalette.h"

// Architectural materials share one silhouette in every run; their pigments come from the maze.
struct CorridorStyle {
    Color plaster, panel, trim, trimLight, trimShade, brass;
    Color ceiling, ceilingInset;
};

// Derive plaster, panel, trim, brass, and ceiling material roles from the supplied maze palette.
// This pure color conversion allocates no graphics resources and consumes no randomness.
inline CorridorStyle corridorStyle(const MazePalette& palette) {
    CorridorStyle style{};
    style.plaster = ColorLerp(palette.wall, palette.paper, .10f);
    style.panel = ColorLerp(palette.floor, palette.trim, .32f);
    style.trim = ColorLerp(palette.accent, palette.trim, .30f);
    style.trimLight = palette.accent;
    style.trimShade = ColorLerp(style.trim, palette.trim, .55f);
    style.brass = palette.accent;
    style.ceiling = ColorLerp(palette.ceiling, palette.paper, .16f);
    style.ceilingInset = ColorLerp(style.ceiling, palette.trim, .13f);
    return style;
}

// One 1024px field and a 256px border strip share an atlas. Keep UV regions and world-space
// border width centralized so texture creation and corridor mapping cannot drift apart.
namespace CarpetLayout {
inline constexpr int FieldSize = 1024;
inline constexpr int BorderSize = 256;
inline constexpr int BorderOffset = FieldSize + 64;
inline constexpr int AtlasWidth = 2048;
inline constexpr float BorderWidth = .14f;
inline constexpr float FieldU = (float)FieldSize / AtlasWidth;
inline constexpr float BorderStartU = (float)BorderOffset / AtlasWidth;
inline constexpr float BorderU = (float)(BorderOffset + BorderSize) / AtlasWidth;
}
