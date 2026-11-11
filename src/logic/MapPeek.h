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
    static constexpr float ViewSecondsFor(int MazeSize, GameMode Mode = GameMode::Normal) {
        return MazeSize > 30 || Mode == GameMode::ZeroGravity ? ExtendedViewSeconds : ViewSeconds;
    }

    // Clear active viewing and cooldown state and select a duration for the new run's size and
    // mode. Call this when starting or retrying so timing from a previous run cannot carry over.
    void Reset(int MazeSize = 0, GameMode Mode = GameMode::Normal) {
        *this = MapPeek{};
        ViewDuration = ViewSecondsFor(MazeSize, Mode);
    }

    // Advance viewing and cooldown using a finite, nonnegative frame duration, carrying leftover
    // time across their boundary. Start a peek only for a fresh press when ready; callers omit
    // paused frames so pausing cannot recharge it.
    void Update(float Delta, bool Pressed) {
        if (!std::isfinite(Delta) || Delta < 0)
            return;
        double Remaining = Delta;
        if (ViewRemaining > 0) {
            const double Used = std::min(Remaining, ViewRemaining);
            ViewRemaining -= Used;
            Remaining -= Used;
            if (ViewRemaining == 0)
                CooldownRemaining = CooldownSeconds;
        }
        CooldownRemaining = std::max(0.0, CooldownRemaining - Remaining);
        if (Pressed && Ready())
            ViewRemaining = ViewDuration;
    }

    // Report whether the map's viewing interval is still active. Rendering can read this without
    // consuming time; only update changes the remaining duration.
    bool Visible() const {
        return ViewRemaining > 0;
    }
    // Allow a new peek only when the map is hidden and its cooldown has reached zero. Repeated
    // requests during a visible peek cannot extend its duration.
    bool Ready() const {
        return !Visible() && CooldownRemaining == 0;
    }
    // Return the remaining visible-map duration in seconds, converting the internally accumulated
    // double to float. This read-only value can drive HUD progress without advancing its timer.
    float ViewTimeLeft() const {
        return (float)ViewRemaining;
    }
    // Return seconds until another peek is available, using zero while no cooldown is pending.
    // This read-only value lets the HUD explain why a new map request is unavailable.
    float CooldownTimeLeft() const {
        return (float)CooldownRemaining;
    }

private:
    double ViewDuration = ViewSeconds;
    double ViewRemaining = 0;
    double CooldownRemaining = 0;
};
