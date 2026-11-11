#pragma once

#include "MenuJourney.h"
#include <functional>
#include <memory>

// All menu screens share the corridor artwork and shader, released with the last screen owner.
class MenuBackground {
public:
    // Reuse the existing scene through a weak shared cache, creating it only when no menu
    // currently owns one. Each backdrop keeps a shared_ptr so textures and journey state survive
    // until the final owner is destroyed.
    MenuBackground();
    // Render the shared corridor at its current journey position, blending source and destination
    // artwork and applying the route's zoom and turn. Drawing reads animation state; update is
    // responsible for advancing time.
    void Draw(float CurrentTime) const;
    // Request a route to a destination through the shared MenuJourney. Return whether the trip
    // was accepted so Application can defer publishing its pending screen until travel finishes.
    bool Navigate(MenuPlace Destination);
    // Request an opening or closing detail animation on the shared journey. Existing active
    // travel and redundant requests are ignored by MenuJourney to keep navigation deterministic.
    void Detail(bool CurrentOpen);
    // Immediately select a corridor stop through the shared journey, clearing its detail and
    // travel state. Use this when returning from a run so the backdrop agrees with the visible
    // menu.
    void Snap(MenuPlace CurrentPlace);
    // Advance the shared journey using a frame duration in seconds. Invalid or nonpositive
    // durations are ignored by MenuJourney, and screen code controls when animation time is
    // allowed to pass.
    void Update(float Delta);
    // Expose whether the shared journey is active without modifying it. Application uses this to
    // keep page input frozen during route or detail animations.
    bool Moving() const;
    // Expose the journey's midpoint decision for choosing which foreground menu to draw. This
    // does not finish navigation or advance animation timing.
    bool DestinationVisible() const;
    // Capture foreground UI into a reusable render target and composite it with the journey's
    // opacity. Keep corridor artwork outside that capture so its turn and grade continue
    // independently behind text.
    void DrawOverlay(const std::function<void()>& DrawUi) const;

private:
    struct Scene;
    std::shared_ptr<Scene> CurrentScene;
};
