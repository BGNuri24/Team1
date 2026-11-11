#pragma once
#include "../data/MazeTheme.h"
#include "../data/RandomSeed.h"
#include <optional>

// Map a uniform draw into a theme index while skipping the previous theme when one exists. The
// caller supplies a draw in the reduced range, keeping all remaining themes equally likely.
inline constexpr MazeTheme ThemeFromDraw(int CurrentDraw, int Previous) {
    return static_cast<MazeTheme>(Previous >= 0 && CurrentDraw >= Previous ? CurrentDraw + 1 : CurrentDraw);
}

// Choose a reproducible palette from a seed mixed separately from maze carving. This helper
// supports optional palette collections; current ordinary run creation uses ActiveMazeTheme
// instead.
inline MazeTheme ThemeForSeed(std::uint32_t CurrentSeed) {
    std::mt19937 Generator(CurrentSeed ^ 0x9e3779b9u);
    return static_cast<MazeTheme>(
        std::uniform_int_distribution<int>(0, MazeThemeCount - 1)(Generator));
}

// Keep non-repeating selection in an explicit application-owned object rather than hidden globals.
class MazeThemeSelector {
public:
    // Seed an independent generator for theme choices and start with no previous selection.
    // Keeping this state in an object makes repeated choices reproducible without modifying
    // maze-generation randomness.
    explicit MazeThemeSelector(std::uint32_t CurrentSeed = RandomSeed()) : Generator(CurrentSeed) {}
    // Choose uniformly from themes other than the previous selection, then remember the result
    // for the next draw. The first call can choose any theme because no prior selection exists.
    MazeTheme Next() {
        const int CurrentDraw = std::uniform_int_distribution<int>(
            0, MazeThemeCount - (Previous >= 0 ? 2 : 1))(Generator);
        const MazeTheme CurrentSelected = ThemeFromDraw(CurrentDraw, Previous);
        Previous = static_cast<int>(CurrentSelected);
        return CurrentSelected;
    }

private:
    std::mt19937 Generator;
    int Previous = -1;
};

// Hold the menu preview until a run consumes it. Reopening menus cannot skip a theme, while a
// restart can start a fresh run without visiting the menus first.
class RunThemeSelector {
public:
    // Create a run-level selector with its own seeded sequence and no prepared preview. Menu
    // previews and run starts share this state so opening menus cannot silently skip choices.
    explicit RunThemeSelector(std::uint32_t CurrentSeed = RandomSeed()) : Selector(CurrentSeed) {}
    // Prepare one theme only when no preview is cached, then return that same value on repeated
    // calls. The preview remains reserved until startRun consumes it.
    MazeTheme Preview() {
        if (!Prepared)
            Prepared = Selector.Next();
        return *Prepared;
    }
    // Return the prepared preview, creating one if necessary, and clear its cache for the
    // following run. A retry can therefore consume a fresh theme without first returning through
    // the menus.
    MazeTheme StartRun() {
        const MazeTheme CurrentSelected = Preview();
        Prepared.reset();
        return CurrentSelected;
    }

private:
    MazeThemeSelector Selector;
    std::optional<MazeTheme> Prepared;
};
