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
    bool GoTo(MenuPlace Destination) {
        if (Active || Destination == Current)
            return false;
        Source = Current;
        Target = Destination;
        DetailFrom = CurrentDetail;
        DetailTo = false;
        Elapsed = 0;
        Duration = .90f;
        Active = true;
        return true;
    }

    // Animate opening or closing a detail view at the current stop over 0.38 seconds. Reject
    // redundant changes and changes during another trip, keeping the same source and destination
    // artwork.
    bool ShowDetail(bool CurrentOpen) {
        if (Active || CurrentDetail == CurrentOpen)
            return false;
        Source = Target = Current;
        DetailFrom = CurrentDetail;
        DetailTo = CurrentOpen;
        Elapsed = 0;
        Duration = .38f;
        Active = true;
        return true;
    }

    // Advance an active journey by a positive finite delta in seconds, clamping at its duration.
    // On completion, publish the destination and detail state and clear the active flag.
    void Update(float Delta) {
        if (!Active || !std::isfinite(Delta) || Delta <= 0)
            return;
        Elapsed = std::min(Duration, Elapsed + Delta);
        if (Elapsed >= Duration) {
            Current = Target;
            CurrentDetail = DetailTo;
            Active = false;
        }
    }

    // Place the journey at a menu stop immediately, canceling travel and clearing detail state.
    // Returns from gameplay use this to restore the appropriate backdrop without replaying a
    // corridor trip.
    void Snap(MenuPlace CurrentPlace) {
        Current = Source = Target = CurrentPlace;
        CurrentDetail = DetailFrom = DetailTo = false;
        Elapsed = Duration;
        Active = false;
    }

    // Report whether a route or detail animation is still active. The application uses this flag
    // to defer regular screen input until the journey finishes.
    bool Moving() const { return Active; }
    // Return the trip's source while moving, otherwise the current stop. Backdrop drawing uses
    // this stable origin when blending or turning between menu views.
    MenuPlace From() const { return Active ? Source : Current; }
    // Return the trip's target while moving, otherwise the current stop. This lets rendering
    // select destination artwork without committing navigation early.
    MenuPlace To() const { return Active ? Target : Current; }
    // Return elapsed travel as a zero-to-one fraction, or one when no trip is active. This
    // accessor reads timing without advancing it and is shared by fade and turn calculations.
    float Progress() const { return Active ? Elapsed / Duration : 1.0f; }
    // Report whether the journey has reached its midpoint, when the foreground switches to the
    // destination page. This affects drawing only; final screen navigation waits until travel
    // ends.
    bool DestinationVisible() const { return Progress() >= .50f; }
    // Identify a detail animation by matching source and target stops. Such a move changes zoom
    // while retaining the same menu page instead of fading between destinations.
    bool DetailMove() const { return Source == Target; }
    // Classify returns toward the entrance or an earlier selection page as backward travel. The
    // backdrop uses this route information to choose a consistent zoom direction.
    bool Backwards() const {
        return Target == MenuPlace::Entrance ||
               (Source == MenuPlace::Levels && Target == MenuPlace::Players) ||
               (Source == MenuPlace::Preparing && Target == MenuPlace::Levels);
    }
    // Return the smooth interpolated amount of an opening or closing detail view. While idle,
    // return its settled zero-or-one state so the backdrop retains the chosen zoom.
    float DetailAmount() const {
        return Active ? (float)DetailFrom + ((float)DetailTo - DetailFrom) * Smooth(Progress())
                      : (float)CurrentDetail;
    }
    // Compute foreground opacity for the current route: preserve detail-page visibility, hide the
    // exit page, and fade ordinary pages around travel. Smooth transitions avoid abrupt text
    // changes during corridor turns.
    float UiOpacity() const {
        if (!Active)
            return Current == MenuPlace::Exit ? 0.0f : 1.0f;
        if (DetailMove())
            return 1;
        const float CurrentT = Progress();
        if (Target == MenuPlace::Exit)
            return 1 - Smooth(CurrentT / .40f);
        return CurrentT < .5f ? 1 - Smooth(CurrentT / .26f) : Smooth((CurrentT - .72f) / .28f);
    }

    // Choose the signed turn direction for the current route, returning zero for idle or
    // detail-only motion. The backdrop combines this route-specific sign with travel progress to
    // animate yaw.
    int Turn() const {
        if (!Active || DetailMove())
            return 0;
        if (Target == MenuPlace::Controls ||
            (Source == MenuPlace::Levels && Target == MenuPlace::Players) ||
            Target == MenuPlace::Preparing)
            return -1;
        if (Source == MenuPlace::Controls || Target == MenuPlace::Levels ||
            Source == MenuPlace::Preparing)
            return 1;
        return 0;
    }

    // Clamp a scalar to zero through one and apply cubic smoothstep easing. The result starts and
    // ends with zero slope, making fades and zooms settle without abrupt velocity changes.
    static float Smooth(float Value) {
        const float CurrentT = std::clamp(Value, 0.0f, 1.0f);
        return CurrentT * CurrentT * (3 - 2 * CurrentT);
    }

private:
    MenuPlace Current = MenuPlace::Entrance, Source = Current, Target = Current;
    bool Active = false, CurrentDetail = false, DetailFrom = false, DetailTo = false;
    float Elapsed = 0, Duration = .90f;
};
