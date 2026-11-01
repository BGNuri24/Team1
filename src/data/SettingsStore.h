#pragma once
#include <optional>

// Read and save global preferences in save_data/settings.txt independently of player profiles and
// scores.
class SettingsStore {
public:
    // Read the stored percentage, returning no value when the selected settings file is unavailable or
    // malformed.
    std::optional<int> loadMouseSensitivity() const;
    // Persist the supplied percentage without changing profile records, returning whether the file
    // replacement succeeded.
    bool saveMouseSensitivity(int percent) const;
};
