#pragma once
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"

class ControlsScreen {
public:
    // Handle confirmation or back navigation from the controls page and return its requested
    // screen. This page presents instructions and does not modify gameplay bindings.
    Screen update();
    // Render the controls guide over its shared menu backdrop, grouping keys with their gameplay
    // actions. Drawing reads the current theme and does not consume input or advance simulation.
    void draw() const;

private:
    bool ceilingPreview = false;
    MenuBackground background;
    MenuTypography type;
};
