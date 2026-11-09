#pragma once

#include "../data/Validation.h"

struct Difficulty {
    const char* name;
    // Number of cells along each side of the square maze.
    int size;
    // Time available to reach the exit, measured in seconds.
    float seconds;
};

const int DIFFICULTY_COUNT = 3;

// Borrow the dedicated Zero Gravity preset, including its name, maze size, and time limit. The
// function-local static keeps the returned reference valid for the lifetime of the program.
inline const Difficulty& getZeroGravityLevel() {
    static const Difficulty level = { "Zero Gravity", 25, 180.0f };
    return level;
}

// Borrow one of the Normal presets by zero-based index. The caller must supply an index below
// DIFFICULTY_COUNT; this accessor performs no bounds checking or difficulty selection.
inline const Difficulty& getDifficulty(int index) {
    static const Difficulty levels[DIFFICULTY_COUNT] = {
        {"Easy",   15, 180.0f},
        {"Medium", 25, 150.0f},
        {"Hard",   40, 120.0f}
    };
    return levels[index];
}

// Calculate a custom level's time allowance as five seconds per side-length cell. Callers
// validate the size separately; this formula does not construct or inspect the maze.
inline float customSeconds(int size) {
    return size * 5.0f;
}
