#pragma once
#include "MazePalette.h"

// Architectural materials share one silhouette in every run; their pigments come from the maze.
struct CorridorStyle {
    Color plaster, panel, trim, trimLight, trimShade;
    Color carpet, thread, ceiling, ceilingInset;
};

inline CorridorStyle corridorStyle(const MazePalette& palette) {
    CorridorStyle style{};
    style.plaster = ColorLerp(palette.ceiling, palette.paper, .48f);
    style.panel = ColorLerp(palette.wall, palette.pattern, .20f);
    style.trim = ColorLerp(palette.accent, palette.lamp, .40f);
    style.trimLight = ColorLerp(style.trim, palette.paper, .42f);
    style.trimShade = ColorLerp(style.trim, palette.trim, .48f);
    style.carpet = ColorLerp(palette.wall, palette.floor, .28f);
    style.thread = ColorLerp(style.trim, palette.paper, .18f);
    style.ceiling = ColorLerp(style.plaster, palette.ceiling, .20f);
    style.ceilingInset = ColorLerp(style.ceiling, palette.wall, .14f);
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
