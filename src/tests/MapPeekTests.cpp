#include "../logic/MapPeek.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
// Throw a descriptive exception when map timing violates its expected contract. The input-suite
// entry point reports the failure without relying on disabled release assertions.
void requireMap(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace

// Check map duration, cooldown, repeated presses, large frame intervals, and new-run resets for
// supported sizes and modes. Drive pure state directly so the results are independent of
// rendering speed.
void runMapPeekTests() {
    MapPeek peek;
    requireMap(peek.ready() && !peek.visible(), "a new run must start hidden and ready to peek");
    peek.update(0, true);
    requireMap(peek.visible() && !peek.ready() && peek.viewTimeLeft() == 1,
               "a fresh request must reveal the map for one second");
    for (int frame = 0; frame < 7; ++frame) {
        peek.update(.125f, true);
        requireMap(peek.visible(), "extra presses must not close the map prematurely");
    }
    peek.update(.125f, true);
    requireMap(!peek.visible() && !peek.ready() && peek.cooldownTimeLeft() == 3,
               "extra presses must not extend the one-second view or bypass cooldown");
    peek.update(.25f, true);
    requireMap(!peek.visible() && peek.cooldownTimeLeft() == 2.75f,
               "requests during cooldown must neither reopen nor restart the cooldown");
    peek.update(2.75f, false);
    requireMap(peek.ready() && !peek.visible(),
               "expired requests must not automatically reopen the map when cooldown ends");
    peek.update(0, true);
    requireMap(peek.visible(), "a new press must work after the full cooldown");

    MapPeek whole, subdivided;
    whole.update(0, true);
    subdivided.update(0, true);
    whole.update(3.75f, false);
    for (int frame = 0; frame < 30; ++frame)
        subdivided.update(.125f, false);
    requireMap(
        !whole.visible() && !subdivided.visible() && whole.cooldownTimeLeft() == .25f &&
            subdivided.cooldownTimeLeft() == .25f,
        "slow frames must carry time from the view into cooldown without changing its length");
    whole.update(.25f, false);
    subdivided.update(.25f, false);
    requireMap(whole.ready() && subdivided.ready(),
               "one second visible plus three seconds cooldown must permit the next request");
    whole.update(0, true);
    whole.update(10, false);
    requireMap(whole.ready() && !whole.visible(), "a long frame must not leave the map stuck open");

    peek.reset();
    peek.update(0, true);
    for (float delta :
         {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        peek.update(delta, true);
        requireMap(peek.visible() && peek.viewTimeLeft() == 1,
                   "invalid timing must not corrupt or extend map visibility");
    }
    peek.reset();
    requireMap(peek.ready() && !peek.visible() && peek.cooldownTimeLeft() == 0,
               "restarting a run must clear the old map view and cooldown");
    peek.update(0, true);
    peek.update(1, false);
    peek.reset();
    requireMap(peek.ready() && !peek.visible(), "restart must clear a cooldown as well as a view");

    struct Case {
        int size;
        GameMode mode;
        float seconds;
    };
    for (const Case test : {Case{15, GameMode::Normal, 1},
                            {25, GameMode::Normal, 1},
                            {30, GameMode::Normal, 1},
                            {31, GameMode::Normal, 2},
                            {40, GameMode::Normal, 2},
                            {50, GameMode::Normal, 2},
                            {25, GameMode::ZeroGravity, 2}}) {
        peek.reset(test.size, test.mode);
        requireMap(peek.ready() && !peek.visible(), "changing level must reset the map state");
        peek.update(0, true);
        requireMap(peek.viewTimeLeft() == test.seconds,
                   "map duration must match grid size and gravity mode");
        peek.update(test.seconds - .125f, true);
        requireMap(peek.visible() && peek.viewTimeLeft() == .125f,
                   "repeated requests must not extend either map duration");
        peek.update(.125f, false);
        requireMap(!peek.visible() && peek.cooldownTimeLeft() == 3,
                   "both peek durations must start the same cooldown only after closing");
        peek.update(3, false);
        requireMap(peek.ready(), "longer peeks must retain the three-second cooldown");
        peek.update(0, true);
        requireMap(peek.viewTimeLeft() == test.seconds,
                   "later peeks must retain the run's configured duration");
    }

    whole.reset(40);
    subdivided.reset(40);
    whole.update(0, true);
    subdivided.update(0, true);
    whole.update(4.75f, false);
    for (int frame = 0; frame < 38; ++frame)
        subdivided.update(.125f, false);
    requireMap(!whole.visible() && !subdivided.visible() && whole.cooldownTimeLeft() == .25f &&
                   subdivided.cooldownTimeLeft() == .25f,
               "extended peeks must carry slow-frame time into cooldown correctly");
    peek.reset(15);
    peek.update(0, true);
    requireMap(peek.viewTimeLeft() == 1,
               "returning to a smaller normal maze must restore one second");
}
