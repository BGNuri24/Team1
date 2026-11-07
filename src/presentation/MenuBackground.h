#pragma once

#include "MenuJourney.h"
#include <functional>
#include <memory>

// All menu screens share the corridor artwork and shader, released with the last screen owner.
class MenuBackground {
public:
    MenuBackground();
    void draw(float time) const;
    bool navigate(MenuPlace destination);
    void detail(bool open);
    void snap(MenuPlace place);
    void update(float delta);
    bool moving() const;
    bool destinationVisible() const;
    // Capture only the foreground UI while the shared corridor continues its turn behind it.
    void drawOverlay(const std::function<void()>& drawUi) const;

private:
    struct Scene;
    std::shared_ptr<Scene> scene;
};
