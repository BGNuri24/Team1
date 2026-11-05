#include "MazePalette.h"

namespace {
    // Blend palette swatches without changing random state or relying on a graphics context.
    constexpr Color mix(Color a, Color b, float amount) {
        return {(unsigned char)(a.r+(b.r-a.r)*amount), (unsigned char)(a.g+(b.g-a.g)*amount),
            (unsigned char)(a.b+(b.b-a.b)*amount),255};
    }
    constexpr Color Ivory = {235,231,218,255};
    constexpr Color Black = {0,0,0,255};

    // Assign material roles while retaining the supplied swatches as the dominant visible colors.
    constexpr MazePalette palette(const char* name, std::array<Color,5> colors, int wall, int pattern,
        int accent, int trim, int ceiling, Color lamp) {
        return {name,colors,colors[wall],colors[pattern],mix(colors[accent],Ivory,.25f),
            mix(colors[trim],Black,.20f),mix(colors[trim],Black,.48f),
            mix(colors[ceiling],Ivory,.12f),lamp,mix(colors[trim],Black,.83f),Ivory};
    }

    constexpr MazePalette Palettes[MazeThemeCount] = {
        palette("SLATE STUDY", {{{32,32,40,255},{49,56,71,255},{58,59,78,255},{63,68,89,255},{77,84,104,255}}},
            3,2,4,1,4,{210,220,242,255}),
        palette("TEAL GALLERY", {{{0,135,123,255},{0,87,77,255},{38,38,38,255},{54,54,54,255},{153,153,153,255}}},
            0,1,4,2,4,{255,230,188,255}),
        palette("CRIMSON HALL", {{{28,37,46,255},{17,21,27,255},{102,28,29,255},{75,21,24,255},{26,31,39,255}}},
            2,3,0,1,0,{255,216,184,255}),
        palette("PLUM LOUNGE", {{{41,31,46,255},{80,56,70,255},{106,72,81,255},{112,51,49,255},{80,31,29,255}}},
            2,1,3,0,1,{249,221,206,255}),
        palette("SAGE ATRIUM", {{{130,166,156,255},{60,58,61,255},{166,156,125,255},{212,212,204,255},{113,125,146,255}}},
            0,2,4,1,3,{255,240,210,255})
    };
}

// Keep palette lookup safe when a caller passes a theme outside the supported enum range.
const MazePalette& mazePalette(MazeTheme theme) {
    const int index = static_cast<int>(theme);
    return Palettes[index >= 0 && index < MazeThemeCount ? index : 0];
}
