#include "GameSettings.h"
#include <algorithm>

GameSettings::GameSettings() {
    const auto saved = store.loadMouseSensitivity();
    if (saved && *saved >= MinSensitivity && *saved <= MaxSensitivity) percent = *saved;
}

int GameSettings::sensitivityPercent() const { return percent; }
float GameSettings::sensitivityMultiplier() const { return percent / 100.0f; }

void GameSettings::setSensitivityPercent(int value) {
    value = std::clamp(value, MinSensitivity, MaxSensitivity);
    if (value == percent) return;
    percent = value;
    dirty = true;
}

void GameSettings::resetSensitivity() { setSensitivityPercent(DefaultSensitivity); }

bool GameSettings::save() {
    if (!dirty) return true;
    if (!store.saveMouseSensitivity(percent)) return false;
    dirty = false;
    return true;
}

bool GameSettings::hasUnsavedChanges() const { return dirty; }
