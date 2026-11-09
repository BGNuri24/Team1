#pragma once
#include "../data/MazeTheme.h"
#include "../data/RandomSeed.h"
#include <optional>

// Map a uniform draw into a theme index while skipping the previous theme when one exists. The
// caller supplies a draw in the reduced range, keeping all remaining themes equally likely.
inline constexpr MazeTheme themeFromDraw(int draw, int previous) {
    return static_cast<MazeTheme>(previous >= 0 && draw >= previous ? draw + 1 : draw);
}

// Choose a reproducible palette from a seed mixed separately from maze carving. This helper
// supports optional palette collections; current ordinary run creation uses ActiveMazeTheme
// instead.
inline MazeTheme themeForSeed(std::uint32_t seed) {
    std::mt19937 generator(seed ^ 0x9e3779b9u);
    return static_cast<MazeTheme>(
        std::uniform_int_distribution<int>(0, MazeThemeCount - 1)(generator));
}

// Keep non-repeating selection in an explicit application-owned object rather than hidden globals.
class MazeThemeSelector {
public:
    // Seed an independent generator for theme choices and start with no previous selection.
    // Keeping this state in an object makes repeated choices reproducible without modifying
    // maze-generation randomness.
    explicit MazeThemeSelector(std::uint32_t seed = randomSeed()) : generator(seed) {}
    // Choose uniformly from themes other than the previous selection, then remember the result
    // for the next draw. The first call can choose any theme because no prior selection exists.
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
    // Create a run-level selector with its own seeded sequence and no prepared preview. Menu
    // previews and run starts share this state so opening menus cannot silently skip choices.
    explicit RunThemeSelector(std::uint32_t seed = randomSeed()) : selector(seed) {}
    // Prepare one theme only when no preview is cached, then return that same value on repeated
    // calls. The preview remains reserved until startRun consumes it.
    MazeTheme preview() {
        if (!prepared)
            prepared = selector.next();
        return *prepared;
    }
    // Return the prepared preview, creating one if necessary, and clear its cache for the
    // following run. A retry can therefore consume a fresh theme without first returning through
    // the menus.
    MazeTheme startRun() {
        const MazeTheme selected = preview();
        prepared.reset();
        return selected;
    }

private:
    MazeThemeSelector selector;
    std::optional<MazeTheme> prepared;
};
