#include "MazePalette.h"

namespace {
// Blend palette colors with the requested amount to derive related material roles. This helper
// performs color arithmetic only and does not change the active UI theme or gameplay state.
constexpr Color Mix(Color CurrentA, Color B, float Amount) {
    return {(unsigned char)(CurrentA.r + (B.r - CurrentA.r) * Amount),
            (unsigned char)(CurrentA.g + (B.g - CurrentA.g) * Amount),
            (unsigned char)(CurrentA.b + (B.b - CurrentA.b) * Amount), 255};
}
constexpr Color Ivory = {235, 231, 218, 255};
constexpr Color Black = {0, 0, 0, 255};

// Build a complete set of material and interface colors from one curated collection of base
// pigments. Derived roles keep walls, trim, floor, fixtures, and text visually related.
constexpr MazePalette Palette(const char* CurrentName, std::array<Color, 5> CurrentColors, int CurrentWall, int Pattern,
                              int CurrentAccent, int Trim, int Ceiling, Color Lamp) {
    return {CurrentName,
            CurrentColors,
            CurrentColors[CurrentWall],
            CurrentColors[Pattern],
            Mix(CurrentColors[CurrentAccent], Ivory, .25f),
            Mix(CurrentColors[Trim], Black, .20f),
            Mix(CurrentColors[Trim], Black, .48f),
            Mix(CurrentColors[Ceiling], Ivory, .12f),
            Lamp,
            Mix(CurrentColors[Trim], Black, .83f),
            Ivory};
}

constexpr MazePalette Palettes[MazeThemeCount] = {Palette("SLATE STUDY",
                                                          {{{32, 32, 40, 255},
                                                            {49, 56, 71, 255},
                                                            {58, 59, 78, 255},
                                                            {63, 68, 89, 255},
                                                            {77, 84, 104, 255}}},
                                                          3, 2, 4, 1, 4, {210, 220, 242, 255}),
                                                  Palette("TEAL GALLERY",
                                                          {{{0, 135, 123, 255},
                                                            {0, 87, 77, 255},
                                                            {38, 38, 38, 255},
                                                            {54, 54, 54, 255},
                                                            {153, 153, 153, 255}}},
                                                          0, 1, 4, 2, 4, {255, 230, 188, 255}),
                                                  Palette("CRIMSON HALL",
                                                          {{{28, 37, 46, 255},
                                                            {17, 21, 27, 255},
                                                            {102, 28, 29, 255},
                                                            {75, 21, 24, 255},
                                                            {26, 31, 39, 255}}},
                                                          2, 3, 0, 1, 0, {255, 216, 184, 255}),
                                                  Palette("PLUM LOUNGE",
                                                          {{{41, 31, 46, 255},
                                                            {80, 56, 70, 255},
                                                            {106, 72, 81, 255},
                                                            {112, 51, 49, 255},
                                                            {80, 31, 29, 255}}},
                                                          2, 1, 3, 0, 1, {249, 221, 206, 255}),
                                                  Palette("SAGE ATRIUM",
                                                          {{{130, 166, 156, 255},
                                                            {60, 58, 61, 255},
                                                            {166, 156, 125, 255},
                                                            {212, 212, 204, 255},
                                                            {113, 125, 146, 255}}},
                                                          0, 2, 4, 1, 3, {255, 240, 210, 255}),
                                                  {"GRAND HOTEL",
                                                   {{{41, 28, 22, 255},
                                                     {86, 52, 38, 255},
                                                     {172, 115, 65, 255},
                                                     {195, 155, 89, 255},
                                                     {243, 223, 183, 255}}},
                                                   {168, 112, 68, 255},
                                                   {139, 87, 49, 255},
                                                   {211, 167, 91, 255},
                                                   {53, 32, 24, 255},
                                                   {38, 23, 17, 255},
                                                   {131, 88, 53, 255},
                                                   {255, 222, 157, 255},
                                                   {19, 11, 8, 255},
                                                   {246, 230, 199, 255}}};
}

// Return the curated material palette for a MazeTheme identifier. Palette lookup is deterministic
// and does not reroll a run, upload textures, or alter the currently applied menu colors.
const MazePalette& GetMazePalette(MazeTheme Theme) {
    const int Index = static_cast<int>(Theme);
    return Palettes[Index >= 0 && Index < MazeThemeCount ? Index : (int)ActiveMazeTheme];
}
