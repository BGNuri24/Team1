#pragma once
#include "../data/MazeTheme.h"
#include <random>

// Map a uniform draw over the remaining themes around the previous index without introducing bias.
inline constexpr MazeTheme themeFromDraw(int draw, int previous) {
    return static_cast<MazeTheme>(previous >= 0 && draw >= previous ? draw + 1 : draw);
}

// Use a separate generator so choosing a palette never consumes the maze-carving random sequence.
inline MazeTheme chooseMazeTheme() {
    static std::mt19937 generator(std::random_device{}());
    static int previous = -1;
    std::uniform_int_distribution<int> draw(0, MazeThemeCount - (previous >= 0 ? 2 : 1));
    const MazeTheme chosen = themeFromDraw(draw(generator), previous);
    previous = static_cast<int>(chosen);
    return chosen;
}
