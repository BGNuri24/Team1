#pragma once
#include <algorithm>
#include <string_view>

inline constexpr int MAX_PLAYERS = 5;
inline constexpr int MAX_NAME_LENGTH = 12;
inline constexpr int MIN_SIZE = 10;
inline constexpr int MAX_SIZE = 50;

// Allow ASCII letters, decimal digits and underscores as individual profile-name characters.
inline bool isNameCharacter(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

// Require a nonempty profile name within the length limit, with every character accepted by
// isNameCharacter.
inline bool isValidPlayerName(std::string_view name) {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH &&
           std::all_of(name.begin(), name.end(), isNameCharacter);
}

// Accept maze side lengths within the inclusive minimum and maximum supported by the game.
inline bool isValidSize(int size) {
    return size >= MIN_SIZE && size <= MAX_SIZE;
}
