#pragma once

#include "../data/Validation.h"

struct Difficulty {
    const char* CurrentName;
    // Number of cells along each side of the square maze.
    int CurrentSize;
    // Time available to reach the exit, measured in seconds.
    float Seconds;
};

const int DifficultyCount = 3;

// Borrow the dedicated Zero Gravity preset, including its name, maze size, and time limit. The
// function-local static keeps the returned reference valid for the lifetime of the program.
inline const Difficulty& GetZeroGravityLevel() {
    static const Difficulty CurrentLevel = { "Zero Gravity", 25, 180.0f };
    return CurrentLevel;
}

// Borrow one of the Normal presets by zero-based index. The caller must supply an index below
// DIFFICULTY_COUNT; this accessor performs no bounds checking or difficulty selection.
inline const Difficulty& GetDifficulty(int Index) {
    static const Difficulty CurrentLevels[DifficultyCount] = {
        {"Easy",   15, 180.0f},
        {"Medium", 25, 150.0f},
        {"Hard",   40, 120.0f}
    };
    return CurrentLevels[Index];
}

// Calculate a custom level's time allowance as five seconds per side-length cell. Callers
// validate the size separately; this formula does not construct or inspect the maze.
inline float CustomSeconds(int CurrentSize) {
    return CurrentSize * 5.0f;
}
