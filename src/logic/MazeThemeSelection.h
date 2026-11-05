#pragma once
#include "../data/MazeTheme.h"
#include "../data/RandomSeed.h"

// Map a uniform draw over the remaining themes around the previous index without introducing bias.
inline constexpr MazeTheme themeFromDraw(int draw, int previous) {
    return static_cast<MazeTheme>(previous >= 0 && draw >= previous ? draw + 1 : draw);
}

// Reproduce a default theme from a run seed without consuming its maze-carving random stream.
inline MazeTheme themeForSeed(std::uint32_t seed) {
    std::mt19937 generator(seed ^ 0x9e3779b9u);
    return static_cast<MazeTheme>(
        std::uniform_int_distribution<int>(0, MazeThemeCount - 1)(generator));
}

// Keep non-repeating selection in an explicit application-owned object rather than hidden globals.
class MazeThemeSelector {
public:
    explicit MazeThemeSelector(std::uint32_t seed = randomSeed()) : generator(seed) {}
    MazeTheme next() {
        const int draw = std::uniform_int_distribution<int>(
            0, MazeThemeCount - (previous >= 0 ? 2 : 1))(generator);
        const MazeTheme selected = themeFromDraw(draw, previous);
        previous = static_cast<int>(selected);
        return selected;
    }

private:
    std::mt19937 generator;
    int previous = -1;
};
