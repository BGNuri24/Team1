#pragma once
#include "MazePalette.h"

// Architectural materials share one silhouette in every run; their pigments come from the maze.
struct CorridorStyle {
    Color Plaster, CurrentPanel, Trim, TrimLight, TrimShade, Brass;
    Color Ceiling, CeilingInset;
};

// Derive plaster, panel, trim, brass, and ceiling material roles from the supplied maze palette.
// This pure color conversion allocates no graphics resources and consumes no randomness.
inline CorridorStyle GetCorridorStyle(const MazePalette& CurrentPalette) {
    CorridorStyle Style{};
    Style.Plaster = ColorLerp(CurrentPalette.CurrentWall, CurrentPalette.CurrentPaper, .10f);
    Style.CurrentPanel = ColorLerp(CurrentPalette.CurrentFloor, CurrentPalette.Trim, .32f);
    Style.Trim = ColorLerp(CurrentPalette.CurrentAccent, CurrentPalette.Trim, .30f);
    Style.TrimLight = CurrentPalette.CurrentAccent;
    Style.TrimShade = ColorLerp(Style.Trim, CurrentPalette.Trim, .55f);
    Style.Brass = CurrentPalette.CurrentAccent;
    Style.Ceiling = ColorLerp(CurrentPalette.Ceiling, CurrentPalette.CurrentPaper, .16f);
    Style.CeilingInset = ColorLerp(Style.Ceiling, CurrentPalette.Trim, .13f);
    return Style;
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
