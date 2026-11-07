#pragma once
#include "../data/MazeTheme.h"
#include "../data/RandomSeed.h"
#include <optional>

// Map a uniform draw over the remaining themes around the previous index without introducing bias.
inline constexpr MazeTheme themeFromDraw(int draw, int previous) {
    return static_cast<MazeTheme>(previous >= 0 && draw >= previous ? draw + 1 : draw);
}

// Retain an optional seeded palette choice for future collections, separate from maze carving.
// Normal runs currently use ActiveMazeTheme instead of palette rotation.
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

// Hold the menu preview until a run consumes it. Reopening menus cannot skip a theme, while a
// restart can start a fresh run without visiting the menus first.
class RunThemeSelector {
public:
    explicit RunThemeSelector(std::uint32_t seed = randomSeed()) : selector(seed) {}
    MazeTheme preview() {
        if (!prepared)
            prepared = selector.next();
        return *prepared;
    }
    MazeTheme startRun() {
        const MazeTheme selected = preview();
        prepared.reset();
        return selected;
    }

private:
    MazeThemeSelector selector;
    std::optional<MazeTheme> prepared;
};
