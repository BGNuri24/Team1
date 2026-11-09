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

    // Choose the map's visible duration from maze size and mode. Grids larger than 30 cells per
    // side and Zero Gravity use the extended duration; other Normal levels use ViewSeconds.
    static constexpr float viewSecondsFor(int mazeSize, GameMode mode = GameMode::Normal) {
        return mazeSize > 30 || mode == GameMode::ZeroGravity ? ExtendedViewSeconds : ViewSeconds;
    }

    // Clear active viewing and cooldown state and select a duration for the new run's size and
    // mode. Call this when starting or retrying so timing from a previous run cannot carry over.
    void reset(int mazeSize = 0, GameMode mode = GameMode::Normal) {
        *this = MapPeek{};
        viewDuration = viewSecondsFor(mazeSize, mode);
    }

    // Advance viewing and cooldown using a finite, nonnegative frame duration, carrying leftover
    // time across their boundary. Start a peek only for a fresh press when ready; callers omit
    // paused frames so pausing cannot recharge it.
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

    // Report whether the map's viewing interval is still active. Rendering can read this without
    // consuming time; only update changes the remaining duration.
    bool visible() const {
        return viewRemaining > 0;
    }
    // Allow a new peek only when the map is hidden and its cooldown has reached zero. Repeated
    // requests during a visible peek cannot extend its duration.
    bool ready() const {
        return !visible() && cooldownRemaining == 0;
    }
    // Return the remaining visible-map duration in seconds, converting the internally accumulated
    // double to float. This read-only value can drive HUD progress without advancing its timer.
    float viewTimeLeft() const {
        return (float)viewRemaining;
    }
    // Return seconds until another peek is available, using zero while no cooldown is pending.
    // This read-only value lets the HUD explain why a new map request is unavailable.
    float cooldownTimeLeft() const {
        return (float)cooldownRemaining;
    }

private:
    double viewDuration = ViewSeconds;
    double viewRemaining = 0;
    double cooldownRemaining = 0;
};
