#pragma once
#include "../data/SettingsStore.h"

class GameSettings {
public:
    static constexpr int MinSensitivity = 25;
    static constexpr int MaxSensitivity = 300;
    static constexpr int DefaultSensitivity = 100;
    static constexpr int SensitivityStep = 5;

    GameSettings();
    int sensitivityPercent() const;
    float sensitivityMultiplier() const;
    void setSensitivityPercent(int percent);
    void resetSensitivity();
    bool save();
    bool hasUnsavedChanges() const;

private:
    SettingsStore store;
    int percent = DefaultSensitivity;
    bool dirty = false;
};
