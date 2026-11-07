#pragma once
#include "GameMode.h"
#include "StorageResult.h"
#include <filesystem>

// Persist profiles and scores through short-lived connections; migrations run only during
// initialization.
class GameDatabase {
public:
    static constexpr const char* DefaultPath = "save_data/maze_game.db";
    explicit GameDatabase(std::filesystem::path path = DefaultPath);
    // Initialize or upgrade storage and return its committed profile list.
    ProfileResult initialize();
    ProfileResult addPlayer(const std::string& player);
    ProfileResult removePlayer(const std::string& player);
    BestTimeResult bestTime(const std::string& player, int size, GameMode mode) const;
    RecordResult record(const std::string& player, int size, float seconds, GameMode mode);

private:
    std::filesystem::path path;
    bool ready = false;
};
