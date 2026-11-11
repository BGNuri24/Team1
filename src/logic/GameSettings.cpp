#include "GameSettings.h"
#include <algorithm>

// Load the saved mouse-sensitivity percentage when it is inside the supported range. Otherwise
// retain the default, leaving the dirty flag clear because no live user change has occurred.
GameSettings::GameSettings() {
    const auto Saved = CurrentStore.LoadMouseSensitivity();
    if (Saved && *Saved >= MinSensitivity && *Saved <= MaxSensitivity) Percent = *Saved;
}

// Return the current live sensitivity as an integer percentage within the supported limits. The
// pause menu uses this display value; input uses sensitivityMultiplier instead.
int GameSettings::SensitivityPercent() const { return Percent; }
// Convert the live percentage into the floating-point mouse-delta scale consumed by readInput. A
// stored value of 100 produces multiplier 1.0 and preserves the baseline look speed.
float GameSettings::SensitivityMultiplier() const { return Percent / 100.0f; }

// Clamp the requested percentage to the supported range and update the live preference only when
// the effective value changes. Mark changed preferences dirty so save can persist them later.
void GameSettings::SetSensitivityPercent(int Value) {
    Value = std::clamp(Value, MinSensitivity, MaxSensitivity);
    if (Value == Percent) return;
    Percent = Value;
    Dirty = true;
}

// Restore DefaultSensitivity through the normal setter. This retains clamping and dirty tracking,
// allowing the pause menu to save the reset using the same path as any adjustment.
void GameSettings::ResetSensitivity() { SetSensitivityPercent(DefaultSensitivity); }

// Skip disk work when nothing changed; otherwise ask SettingsStore to persist the live
// percentage. Clear the dirty flag only on success so a failed write can be retried without
// losing the chosen value.
bool GameSettings::Save() {
    if (!Dirty) return true;
    if (!CurrentStore.SaveMouseSensitivity(Percent)) return false;
    Dirty = false;
    return true;
}

// Report whether the live sensitivity still differs from its last successfully saved state. A
// failed save keeps this flag set so the UI can warn and retry.
bool GameSettings::HasUnsavedChanges() const { return Dirty; }
