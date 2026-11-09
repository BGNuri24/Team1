#pragma once
#include "../data/SettingsStore.h"

class GameSettings {
public:
    static constexpr int MinSensitivity = 25;
    static constexpr int MaxSensitivity = 300;
    static constexpr int DefaultSensitivity = 100;
    static constexpr int SensitivityStep = 5;

    // Load the saved mouse-sensitivity percentage when it is inside the supported range.
    // Otherwise retain the default, leaving the dirty flag clear because no live user change has
    // occurred.
    GameSettings();
    // Return the current live sensitivity as an integer percentage within the supported limits.
    // The pause menu uses this display value; input uses sensitivityMultiplier instead.
    int sensitivityPercent() const;
    // Convert the live percentage into the floating-point mouse-delta scale consumed by
    // readInput. A stored value of 100 produces multiplier 1.0 and preserves the baseline look
    // speed.
    float sensitivityMultiplier() const;
    // Clamp the requested percentage to the supported range and update the live preference only
    // when the effective value changes. Mark changed preferences dirty so save can persist them
    // later.
    void setSensitivityPercent(int percent);
    // Restore DefaultSensitivity through the normal setter. This retains clamping and dirty
    // tracking, allowing the pause menu to save the reset using the same path as any adjustment.
    void resetSensitivity();
    // Skip disk work when nothing changed; otherwise ask SettingsStore to persist the live
    // percentage. Clear the dirty flag only on success so a failed write can be retried without
    // losing the chosen value.
    bool save();
    // Report whether the live sensitivity still differs from its last successfully saved state. A
    // failed save keeps this flag set so the UI can warn and retry.
    bool hasUnsavedChanges() const;

private:
    SettingsStore store;
    int percent = DefaultSensitivity;
    bool dirty = false;
};
