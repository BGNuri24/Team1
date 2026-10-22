#include "Players.h"

Players::Players() : activeIndex(0) {}

int Players::count() const { return store.count(); }

const std::string& Players::name(int i) const { return store.name(i); }

const std::string& Players::activeName() const {
    static const std::string guest = "Guest";
    if (activeIndex < 0 || activeIndex >= store.count()) return guest;
    return store.name(activeIndex);
}

void Players::setActive(int i) {
    if (i >= 0 && i < store.count()) activeIndex = i;
}

bool Players::isValidName(std::string_view name) {
    if (name.empty() || (int)name.size() > MAX_NAME_LENGTH) return false;
    for (char c : name) {
        bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        bool digit = (c >= '0' && c <= '9');
        if (!letter && !digit && c != '_') return false;
    }
    return true;
}

Players::AddResult Players::add(std::string_view name) {
    if (!isValidName(name)) return INVALID;
    if (store.count() >= MAX_PLAYERS) return FULL;
    if (store.contains(name)) return DUPLICATE;
    return store.add(name) ? ADDED : SAVE_FAILED;
}

bool Players::remove(int i) {
    if (i < 0 || i >= store.count()) return false;
    if (!store.remove(i)) return false;
    if (i < activeIndex) activeIndex--;
    if (activeIndex >= store.count()) activeIndex = 0;
    return true;
}

float Players::bestTime(int mazeSize) const {
    return store.bestTime(activeName(), mazeSize);
}

bool Players::recordWin(int mazeSize, float seconds) {
    return store.recordBestTime(activeName(), mazeSize, seconds);
}
