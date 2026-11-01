#pragma once
#include "../data/SettingsStore.h"

class GameSettings {
public:
    static constexpr int MinSensitivity = 25;
    static constexpr int MaxSensitivity = 300;
    static constexpr int DefaultSensitivity = 100;
    static constexpr int SensitivityStep = 5;

    // Load a valid saved preference or retain the default without requesting a disk write.
    GameSettings();
    // Return the live sensitivity percentage within the supported minimum and maximum.
    int sensitivityPercent() const;
    // Return the mouse-input scale, with 1.0 representing 100 percent sensitivity.
    float sensitivityMultiplier() const;
    // Clamp and apply a live preference, marking it for persistence only when its value changes.
    void setSensitivityPercent(int percent);
    // Restore the default preference while retaining the normal change-tracking behavior.
    void resetSensitivity();
    // Save pending changes; report failure without discarding the live preference or its retry flag.
    bool save();
    // Report whether a preference change is still pending a successful save.
    bool hasUnsavedChanges() const;

private:
    SettingsStore store;
    int percent = DefaultSensitivity;
    bool dirty = false;
};
