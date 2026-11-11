#include "../logic/MapPeek.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Throw a descriptive exception when map timing violates its expected contract. The input-suite
// entry point reports the failure without relying on disabled release assertions.
void RequireMap(bool Value, const char* Message) {
    if (!Value)
        throw std::runtime_error(Message);
}
} // namespace

// Check map duration, cooldown, repeated presses, large frame intervals, and new-run resets for
// supported sizes and modes. Drive pure state directly so the results are independent of
// rendering speed.
void RunMapPeekTests() {
    MapPeek Peek;
    RequireMap(Peek.Ready() && !Peek.Visible(), "a new run must start hidden and ready to peek");
    Peek.Update(0, true);
    RequireMap(Peek.Visible() && !Peek.Ready() && Peek.ViewTimeLeft() == 1,
               "a fresh request must reveal the map for one second");
    for (int Frame = 0; Frame < 7; ++Frame) {
        Peek.Update(.125f, true);
        RequireMap(Peek.Visible(), "extra presses must not close the map prematurely");
    }
    Peek.Update(.125f, true);
    RequireMap(!Peek.Visible() && !Peek.Ready() && Peek.CooldownTimeLeft() == 3,
               "extra presses must not extend the one-second view or bypass cooldown");
    Peek.Update(.25f, true);
    RequireMap(!Peek.Visible() && Peek.CooldownTimeLeft() == 2.75f,
               "requests during cooldown must neither reopen nor restart the cooldown");
    Peek.Update(2.75f, false);
    RequireMap(Peek.Ready() && !Peek.Visible(),
               "expired requests must not automatically reopen the map when cooldown ends");
    Peek.Update(0, true);
    RequireMap(Peek.Visible(), "a new press must work after the full cooldown");

    MapPeek Whole, Subdivided;
    Whole.Update(0, true);
    Subdivided.Update(0, true);
    Whole.Update(3.75f, false);
    for (int Frame = 0; Frame < 30; ++Frame)
        Subdivided.Update(.125f, false);
    RequireMap(
        !Whole.Visible() && !Subdivided.Visible() && Whole.CooldownTimeLeft() == .25f &&
            Subdivided.CooldownTimeLeft() == .25f,
        "slow frames must carry time from the view into cooldown without changing its length");
    Whole.Update(.25f, false);
    Subdivided.Update(.25f, false);
    RequireMap(Whole.Ready() && Subdivided.Ready(),
               "one second visible plus three seconds cooldown must permit the next request");
    Whole.Update(0, true);
    Whole.Update(10, false);
    RequireMap(Whole.Ready() && !Whole.Visible(), "a long frame must not leave the map stuck open");

    Peek.Reset();
    Peek.Update(0, true);
    for (float Delta :
         {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        Peek.Update(Delta, true);
        RequireMap(Peek.Visible() && Peek.ViewTimeLeft() == 1,
                   "invalid timing must not corrupt or extend map visibility");
    }
    Peek.Reset();
    RequireMap(Peek.Ready() && !Peek.Visible() && Peek.CooldownTimeLeft() == 0,
               "restarting a run must clear the old map view and cooldown");
    Peek.Update(0, true);
    Peek.Update(1, false);
    Peek.Reset();
    RequireMap(Peek.Ready() && !Peek.Visible(), "restart must clear a cooldown as well as a view");

    struct Case {
        int CurrentSize;
        GameMode Mode;
        float Seconds;
    };
    for (const Case CurrentTest : {Case{15, GameMode::Normal, 1},
                            {25, GameMode::Normal, 1},
                            {30, GameMode::Normal, 1},
                            {31, GameMode::Normal, 2},
                            {40, GameMode::Normal, 2},
                            {50, GameMode::Normal, 2},
                            {25, GameMode::ZeroGravity, 2}}) {
        Peek.Reset(CurrentTest.CurrentSize, CurrentTest.Mode);
        RequireMap(Peek.Ready() && !Peek.Visible(), "changing level must reset the map state");
        Peek.Update(0, true);
        RequireMap(Peek.ViewTimeLeft() == CurrentTest.Seconds,
                   "map duration must match grid size and gravity mode");
        Peek.Update(CurrentTest.Seconds - .125f, true);
        RequireMap(Peek.Visible() && Peek.ViewTimeLeft() == .125f,
                   "repeated requests must not extend either map duration");
        Peek.Update(.125f, false);
        RequireMap(!Peek.Visible() && Peek.CooldownTimeLeft() == 3,
                   "both peek durations must start the same cooldown only after closing");
        Peek.Update(3, false);
        RequireMap(Peek.Ready(), "longer peeks must retain the three-second cooldown");
        Peek.Update(0, true);
        RequireMap(Peek.ViewTimeLeft() == CurrentTest.Seconds,
                   "later peeks must retain the run's configured duration");
    }

    Whole.Reset(40);
    Subdivided.Reset(40);
    Whole.Update(0, true);
    Subdivided.Update(0, true);
    Whole.Update(4.75f, false);
    for (int Frame = 0; Frame < 38; ++Frame)
        Subdivided.Update(.125f, false);
    RequireMap(!Whole.Visible() && !Subdivided.Visible() && Whole.CooldownTimeLeft() == .25f &&
                   Subdivided.CooldownTimeLeft() == .25f,
               "extended peeks must carry slow-frame time into cooldown correctly");
    Peek.Reset(15);
    Peek.Update(0, true);
    RequireMap(Peek.ViewTimeLeft() == 1,
               "returning to a smaller normal maze must restore one second");
}
