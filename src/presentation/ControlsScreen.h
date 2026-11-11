#pragma once
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"

class ControlsScreen {
public:
    // Handle confirmation or back navigation from the controls page and return its requested
    // screen. This page presents instructions and does not modify gameplay bindings.
    Screen Update();
    // Render the controls guide over its shared menu backdrop, grouping keys with their gameplay
    // actions. Drawing reads the current theme and does not consume input or advance simulation.
    void Draw() const;

private:
    bool CeilingPreview = false;
    MenuBackground Background;
    MenuTypography Type;
};
