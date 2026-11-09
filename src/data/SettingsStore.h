#pragma once
#include <optional>

// Read and save global preferences in save_data/settings.txt independently of player profiles and
// scores.
class SettingsStore {
public:
    // Read the primary settings file, consulting its backup only when the primary is absent.
    // Return an empty optional for unavailable or malformed data without creating files or
    // changing preferences.
    std::optional<int> loadMouseSensitivity() const;
    // Write the supplied percentage to a checked temporary file, then replace the settings file
    // using a backup for recovery. Return false if saving fails; profiles and scores are never
    // changed by this operation.
    bool saveMouseSensitivity(int percent) const;
};
