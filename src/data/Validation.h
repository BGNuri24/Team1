#pragma once
#include <algorithm>
#include <string_view>

inline constexpr int MAX_PLAYERS = 5;
inline constexpr int MAX_NAME_LENGTH = 12;
inline constexpr int MIN_SIZE = 10;
inline constexpr int MAX_SIZE = 50;

// Accept only ASCII letters, decimal digits, and underscores for a profile name. The integer
// input also accepts character codes from raylib's text queue without first narrowing them to
// char.
inline bool isNameCharacter(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_';
}

// Require a nonempty name no longer than MAX_NAME_LENGTH and check every character with
// isNameCharacter. The string_view borrows existing text, so validation does not allocate or save
// a profile.
inline bool isValidPlayerName(std::string_view name) {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH &&
           std::all_of(name.begin(), name.end(), isNameCharacter);
}

// Check whether a maze side length is within the inclusive MIN_SIZE and MAX_SIZE limits. This
// validates a proposed value without clamping it or modifying any current level.
inline bool isValidSize(int size) {
    return size >= MIN_SIZE && size <= MAX_SIZE;
}
