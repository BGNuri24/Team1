#pragma once
#include <optional>

// Settings are separate from player profiles and best-time records.
class SettingsStore {
public:
    std::optional<int> loadMouseSensitivity() const;
    bool saveMouseSensitivity(int percent) const;
};
