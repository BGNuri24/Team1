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

// Return the dedicated Zero Gravity preset with its own maze size and time limit.
inline const Difficulty& getZeroGravityLevel() {
    static const Difficulty level = { "Zero Gravity", 25, 180.0f };
    return level;
}

// Return a Normal preset by index; callers must supply an index from zero through DIFFICULTY_COUNT
// minus one.
inline const Difficulty& getDifficulty(int index) {
    static const Difficulty levels[DIFFICULTY_COUNT] = {
        {"Easy",   15, 180.0f},
        {"Medium", 25, 150.0f},
        {"Hard",   40, 120.0f}
    };
    return levels[index];
}

// Assign five seconds per side-length cell when calculating a custom maze's time limit.
inline float customSeconds(int size) {
    return size * 5.0f;
}
