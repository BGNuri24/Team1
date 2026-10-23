#pragma once

const int MIN_SIZE = 10;     // spec: maze from 10x10 ...
const int MAX_SIZE = 50;     // ... to 50x50

struct Difficulty {
    const char* name;
    int size;        // maze is size x size
    float seconds;   // time limit
};

const int DIFFICULTY_COUNT = 3;

inline const Difficulty& getDifficulty(int index) {
    static const Difficulty levels[DIFFICULTY_COUNT] = {
        {"Easy",   15, 180.0f},
        {"Medium", 25, 150.0f},
        {"Hard",   40, 120.0f}
    };
    return levels[index];
}

// time limit for a custom size: bigger maze = more seconds
inline float customSeconds(int size) {
    return size * 5.0f;
}

// validation of a typed maze size
inline bool isValidSize(int size) {
    return size >= MIN_SIZE && size <= MAX_SIZE;
}