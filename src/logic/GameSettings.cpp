#include "GameSettings.h"
#include <algorithm>

// Load a sensitivity inside the supported range, otherwise keep the default without marking settings
// as changed.
GameSettings::GameSettings() {
    const auto saved = store.loadMouseSensitivity();
    if (saved && *saved >= MinSensitivity && *saved <= MaxSensitivity) percent = *saved;
}

// Return the current sensitivity as an integer percentage, where 100 represents the original camera
// speed.
int GameSettings::sensitivityPercent() const { return percent; }
// Convert the stored percentage to the floating-point multiplier consumed by mouse input.
float GameSettings::sensitivityMultiplier() const { return percent / 100.0f; }

// Clamp the requested sensitivity and mark it unsaved only when the effective value changes.
void GameSettings::setSensitivityPercent(int value) {
    value = std::clamp(value, MinSensitivity, MaxSensitivity);
    if (value == percent) return;
    percent = value;
    dirty = true;
}

// Restore the default percentage through the normal setter so change tracking and range rules remain
// consistent.
void GameSettings::resetSensitivity() { setSensitivityPercent(DefaultSensitivity); }

// Persist only changed settings and clear the dirty flag on success; retain the live value and flag
// when saving fails.
bool GameSettings::save() {
    if (!dirty) return true;
    if (!store.saveMouseSensitivity(percent)) return false;
    dirty = false;
    return true;
}

// Report whether the live preference still needs to be written to disk.
bool GameSettings::hasUnsavedChanges() const { return dirty; }
