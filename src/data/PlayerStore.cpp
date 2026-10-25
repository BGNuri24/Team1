#include "PlayerStore.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <utility>

static const char* FILE_NAME = "save_data/players.txt";
static const char* BEST_TIMES_FILE_NAME = "save_data/best_times.txt";

template <typename Records>
static auto findBestTime(Records& records, const std::string& player, int mazeSize) {
    return std::find_if(records.begin(), records.end(), [&](const auto& record) {
        return record.player == player && record.mazeSize == mazeSize;
    });
}

static std::filesystem::path backupPath(const std::filesystem::path& path) {
    std::filesystem::path backup = path;
    backup += ".bak";
    return backup;
}

static void recoverBackup(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path backup = backupPath(path);
    if (!std::filesystem::exists(path, error) && !error &&
        std::filesystem::exists(backup, error) && !error)
        std::filesystem::rename(backup, path, error);
}

template <typename WriteContent>
static bool writeAtomically(const char* filename, WriteContent writeContent) {
    std::filesystem::path path(filename);
    std::error_code directoryError;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), directoryError);
    if (directoryError) return false;
    recoverBackup(path);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    std::filesystem::path backup = backupPath(path);

    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    writeContent(out);
    out.flush();
    if (!out) {
        out.close();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    out.close();
    if (!out) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }

    std::error_code error;
    std::filesystem::remove(backup, error);
    error.clear();
    bool hadOriginal = std::filesystem::exists(path, error);
    if (error) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    if (hadOriginal) {
        std::filesystem::rename(path, backup, error);
        if (error) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    std::filesystem::rename(temporary, path, error);
    if (error) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(backup, path, restoreError);
        }
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    if (hadOriginal) std::filesystem::remove(backup, error);
    return true;
}

PlayerStore::PlayerStore() {
    load();
    loadBestTimes();
}

int PlayerStore::count() const { return (int)names.size(); }

const std::string& PlayerStore::name(int i) const {
    static const std::string empty;
    if (i < 0 || i >= count()) return empty;
    return names[i];
}

bool PlayerStore::contains(const std::string& name) const {
    return std::find(names.begin(), names.end(), name) != names.end();
}

bool PlayerStore::add(const std::string& name) {
    if (!isValidPlayerName(name) || count() >= MAX_PLAYERS || contains(name)) return false;
    names.push_back(name);
    if (save()) return true;
    names.pop_back();
    return false;
}

bool PlayerStore::remove(int i) {
    if (i < 0 || i >= count()) return false;
    std::vector<std::string> previousNames = names;
    std::vector<BestTime> previousTimes = bestTimes;
    const std::string removedName = names[i];
    names.erase(names.begin() + i);
    bestTimes.erase(
        std::remove_if(bestTimes.begin(), bestTimes.end(), [&](const BestTime& record) {
            return record.player == removedName;
        }),
        bestTimes.end());
    if (!saveBestTimes()) {
        names = std::move(previousNames);
        bestTimes = std::move(previousTimes);
        return false;
    }
    if (!save()) {
        names = std::move(previousNames);
        bestTimes = std::move(previousTimes);
        saveBestTimes();
        return false;
    }
    return true;
}

float PlayerStore::bestTime(const std::string& player, int mazeSize) const {
    auto record = findBestTime(bestTimes, player, mazeSize);
    return record == bestTimes.end() ? -1.0f : record->seconds;
}

bool PlayerStore::recordBestTime(const std::string& player, int mazeSize, float seconds) {
    if (!contains(player) || !isValidSize(mazeSize) ||
        !std::isfinite(seconds) || seconds < 0.0f) return false;

    auto record = findBestTime(bestTimes, player, mazeSize);
    if (record != bestTimes.end()) {
        if (seconds >= record->seconds) return false;
        float previousSeconds = record->seconds;
        record->seconds = seconds;
        if (saveBestTimes()) return true;
        record->seconds = previousSeconds;
        return false;
    }

    bestTimes.push_back({player, mazeSize, seconds});
    if (saveBestTimes()) return true;
    bestTimes.pop_back();
    return false;
}

void PlayerStore::load() {
    recoverBackup(FILE_NAME);
    std::ifstream in(FILE_NAME);
    std::string line;
    while (std::getline(in, line) && count() < MAX_PLAYERS) {
        if (isValidPlayerName(line) && !contains(line)) names.push_back(line);
    }
}

bool PlayerStore::save() const {
    return writeAtomically(FILE_NAME, [&](std::ostream& out) {
        for (const std::string& n : names) out << n << '\n';
    });
}

void PlayerStore::loadBestTimes() {
    recoverBackup(BEST_TIMES_FILE_NAME);
    std::ifstream in(BEST_TIMES_FILE_NAME);
    BestTime record;
    while (in >> record.player >> record.mazeSize >> record.seconds) {
        if (!contains(record.player) || !isValidSize(record.mazeSize) || !std::isfinite(record.seconds) ||
            record.seconds < 0.0f) continue;
        auto existing = findBestTime(bestTimes, record.player, record.mazeSize);
        if (existing == bestTimes.end()) bestTimes.push_back(record);
        else if (record.seconds < existing->seconds) existing->seconds = record.seconds;
    }
}

bool PlayerStore::saveBestTimes() const {
    return writeAtomically(BEST_TIMES_FILE_NAME, [&](std::ostream& out) {
        for (const BestTime& record : bestTimes)
            out << record.player << ' ' << record.mazeSize << ' ' << record.seconds << '\n';
    });
}
