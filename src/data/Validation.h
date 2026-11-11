#pragma once
#include <algorithm>
#include <string_view>

inline constexpr int MaxPlayers = 5;
inline constexpr int MaxNameLength = 12;
inline constexpr int MinSize = 10;
inline constexpr int MaxSize = 50;

// Accept only ASCII letters, decimal digits, and underscores for a profile name. The integer
// input also accepts character codes from raylib's text queue without first narrowing them to
// char.
inline bool IsNameCharacter(int Ch) {
    return (Ch >= 'a' && Ch <= 'z') || (Ch >= 'A' && Ch <= 'Z') ||
           (Ch >= '0' && Ch <= '9') || Ch == '_';
}

// Require a nonempty name no longer than MAX_NAME_LENGTH and check every character with
// isNameCharacter. The string_view borrows existing text, so validation does not allocate or save
// a profile.
inline bool IsValidPlayerName(std::string_view CurrentName) {
    return !CurrentName.empty() && CurrentName.size() <= MaxNameLength &&
           std::all_of(CurrentName.begin(), CurrentName.end(), IsNameCharacter);
}

// Check whether a maze side length is within the inclusive MIN_SIZE and MAX_SIZE limits. This
// validates a proposed value without clamping it or modifying any current level.
inline bool IsValidSize(int CurrentSize) {
    return CurrentSize >= MinSize && CurrentSize <= MaxSize;
}
