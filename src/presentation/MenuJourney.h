#pragma once
#include <algorithm>
#include <cmath>

enum class MenuPlace { Entrance, Players, Levels, Preparing, Controls, Exit };

// Pure timing and route state: one confirmed action starts one journey; input cannot replace it
// mid-turn. Rendering and file loading stay outside this class.
class MenuJourney {
public:
    // Start a 0.90-second trip from the current menu stop to destination and close any detail
    // view during travel. Reject a request while already moving or when destination is the
    // current stop.
    bool goTo(MenuPlace destination) {
        if (active || destination == current)
            return false;
        source = current;
        target = destination;
        detailFrom = detail;
        detailTo = false;
        elapsed = 0;
        duration = .90f;
        active = true;
        return true;
    }

    // Animate opening or closing a detail view at the current stop over 0.38 seconds. Reject
    // redundant changes and changes during another trip, keeping the same source and destination
    // artwork.
    bool showDetail(bool open) {
        if (active || detail == open)
            return false;
        source = target = current;
        detailFrom = detail;
        detailTo = open;
        elapsed = 0;
        duration = .38f;
        active = true;
        return true;
    }

    // Advance an active journey by a positive finite delta in seconds, clamping at its duration.
    // On completion, publish the destination and detail state and clear the active flag.
    void update(float delta) {
        if (!active || !std::isfinite(delta) || delta <= 0)
            return;
        elapsed = std::min(duration, elapsed + delta);
        if (elapsed >= duration) {
            current = target;
            detail = detailTo;
            active = false;
        }
    }

    // Place the journey at a menu stop immediately, canceling travel and clearing detail state.
    // Returns from gameplay use this to restore the appropriate backdrop without replaying a
    // corridor trip.
    void snap(MenuPlace place) {
        current = source = target = place;
        detail = detailFrom = detailTo = false;
        elapsed = duration;
        active = false;
    }

    // Report whether a route or detail animation is still active. The application uses this flag
    // to defer regular screen input until the journey finishes.
    bool moving() const { return active; }
    // Return the trip's source while moving, otherwise the current stop. Backdrop drawing uses
    // this stable origin when blending or turning between menu views.
    MenuPlace from() const { return active ? source : current; }
    // Return the trip's target while moving, otherwise the current stop. This lets rendering
    // select destination artwork without committing navigation early.
    MenuPlace to() const { return active ? target : current; }
    // Return elapsed travel as a zero-to-one fraction, or one when no trip is active. This
    // accessor reads timing without advancing it and is shared by fade and turn calculations.
    float progress() const { return active ? elapsed / duration : 1.0f; }
    // Report whether the journey has reached its midpoint, when the foreground switches to the
    // destination page. This affects drawing only; final screen navigation waits until travel
    // ends.
    bool destinationVisible() const { return progress() >= .50f; }
    // Identify a detail animation by matching source and target stops. Such a move changes zoom
    // while retaining the same menu page instead of fading between destinations.
    bool detailMove() const { return source == target; }
    // Classify returns toward the entrance or an earlier selection page as backward travel. The
    // backdrop uses this route information to choose a consistent zoom direction.
    bool backwards() const {
        return target == MenuPlace::Entrance ||
               (source == MenuPlace::Levels && target == MenuPlace::Players) ||
               (source == MenuPlace::Preparing && target == MenuPlace::Levels);
    }
    // Return the smooth interpolated amount of an opening or closing detail view. While idle,
    // return its settled zero-or-one state so the backdrop retains the chosen zoom.
    float detailAmount() const {
        return active ? (float)detailFrom + ((float)detailTo - detailFrom) * smooth(progress())
                      : (float)detail;
    }
    // Compute foreground opacity for the current route: preserve detail-page visibility, hide the
    // exit page, and fade ordinary pages around travel. Smooth transitions avoid abrupt text
    // changes during corridor turns.
    float uiOpacity() const {
        if (!active)
            return current == MenuPlace::Exit ? 0.0f : 1.0f;
        if (detailMove())
            return 1;
        const float t = progress();
        if (target == MenuPlace::Exit)
            return 1 - smooth(t / .40f);
        return t < .5f ? 1 - smooth(t / .26f) : smooth((t - .72f) / .28f);
    }

    // Choose the signed turn direction for the current route, returning zero for idle or
    // detail-only motion. The backdrop combines this route-specific sign with travel progress to
    // animate yaw.
    int turn() const {
        if (!active || detailMove())
            return 0;
        if (target == MenuPlace::Controls ||
            (source == MenuPlace::Levels && target == MenuPlace::Players) ||
            target == MenuPlace::Preparing)
            return -1;
        if (source == MenuPlace::Controls || target == MenuPlace::Levels ||
            source == MenuPlace::Preparing)
            return 1;
        return 0;
    }

    // Clamp a scalar to zero through one and apply cubic smoothstep easing. The result starts and
    // ends with zero slope, making fades and zooms settle without abrupt velocity changes.
    static float smooth(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }

private:
    MenuPlace current = MenuPlace::Entrance, source = current, target = current;
    bool active = false, detail = false, detailFrom = false, detailTo = false;
    float elapsed = 0, duration = .90f;
};
