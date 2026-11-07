#pragma once
#include <algorithm>
#include <cmath>

enum class MenuPlace { Entrance, Players, Levels, Preparing, Controls, Exit };

// Pure timing and route state: one confirmed action starts one journey; input cannot replace it
// mid-turn. Rendering and file loading stay outside this class.
class MenuJourney {
public:
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

    void snap(MenuPlace place) {
        current = source = target = place;
        detail = detailFrom = detailTo = false;
        elapsed = duration;
        active = false;
    }

    bool moving() const { return active; }
    MenuPlace from() const { return active ? source : current; }
    MenuPlace to() const { return active ? target : current; }
    float progress() const { return active ? elapsed / duration : 1.0f; }
    bool destinationVisible() const { return progress() >= .50f; }
    bool detailMove() const { return source == target; }
    bool backwards() const {
        return target == MenuPlace::Entrance ||
               (source == MenuPlace::Levels && target == MenuPlace::Players) ||
               (source == MenuPlace::Preparing && target == MenuPlace::Levels);
    }
    float detailAmount() const {
        return active ? (float)detailFrom + ((float)detailTo - detailFrom) * smooth(progress())
                      : (float)detail;
    }
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

    static float smooth(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }

private:
    MenuPlace current = MenuPlace::Entrance, source = current, target = current;
    bool active = false, detail = false, detailFrom = false, detailTo = false;
    float elapsed = 0, duration = .90f;
};
