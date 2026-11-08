#pragma once
#include "../data/GameMode.h"
#include <algorithm>
#include <cmath>

// Larger grids and Zero Gravity allow a longer peek. Advance only during active gameplay so
// pausing cannot recharge the map or leave it visible indefinitely.
class MapPeek {
public:
    static constexpr float ViewSeconds = 1.0f;
    static constexpr float ExtendedViewSeconds = 2.0f;
    static constexpr float CooldownSeconds = 3.0f;

    static constexpr float viewSecondsFor(int mazeSize, GameMode mode = GameMode::Normal) {
        return mazeSize > 30 || mode == GameMode::ZeroGravity ? ExtendedViewSeconds : ViewSeconds;
    }

    void reset(int mazeSize = 0, GameMode mode = GameMode::Normal) {
        *this = MapPeek{};
        viewDuration = viewSecondsFor(mazeSize, mode);
    }

    void update(float delta, bool pressed) {
        if (!std::isfinite(delta) || delta < 0)
            return;
        double remaining = delta;
        if (viewRemaining > 0) {
            const double used = std::min(remaining, viewRemaining);
            viewRemaining -= used;
            remaining -= used;
            if (viewRemaining == 0)
                cooldownRemaining = CooldownSeconds;
        }
        cooldownRemaining = std::max(0.0, cooldownRemaining - remaining);
        if (pressed && ready())
            viewRemaining = viewDuration;
    }

    bool visible() const {
        return viewRemaining > 0;
    }
    bool ready() const {
        return !visible() && cooldownRemaining == 0;
    }
    float viewTimeLeft() const {
        return (float)viewRemaining;
    }
    float cooldownTimeLeft() const {
        return (float)cooldownRemaining;
    }

private:
    double viewDuration = ViewSeconds;
    double viewRemaining = 0;
    double cooldownRemaining = 0;
};
