#pragma once
#include "MenuBackground.h"
#include "Screen.h"
#include "Ui.h"

class ControlsScreen {
public:
    Screen update();
    void draw() const;

private:
    bool ceilingPreview = false;
    MenuBackground background;
    MenuTypography type;
};
