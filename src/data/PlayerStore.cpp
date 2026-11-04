#include "PlayerStore.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <utility>

static const char* FILE_NAME = "save_data/players.txt";
// Build the sibling backup filename by appending .bak to the complete save-file path.
static std::filesystem::path backupPath(const std::filesystem::path& path) {
    std::filesystem::path backup = path;
    backup += ".bak";
    return backup;
}

// Attempt to restore a backup only when the main save file is absent, using error codes instead of
// filesystem exceptions.
static void recoverBackup(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::path backup = backupPath(path);
    if (!std::filesystem::exists(path, error) && !error &&
        std::filesystem::exists(backup, error) && !error)
        std::filesystem::rename(backup, path, error);
}

// Write complete content to a temporary file, move the original to a backup, and publish the
// replacement; attempt restoration if publication fails.
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

// Load profiles before migration so legacy scores belonging to unknown names can be ignored.
PlayerStore::PlayerStore() {
    load();
    bestTimes.initialize(names);
}

// Return the number of profiles currently held in memory.
int PlayerStore::count() const { return (int)names.size(); }

// Return the indexed profile name, or a stable empty string when the index is outside the profile
// list.
const std::string& PlayerStore::name(int i) const {
    static const std::string empty;
    if (i < 0 || i >= count()) return empty;
    return names[i];
}

// Check for an exact, case-sensitive name match among the saved profiles.
bool PlayerStore::contains(const std::string& name) const {
    return std::find(names.begin(), names.end(), name) != names.end();
}

// Validate and append a unique profile within the limit, rolling back the in-memory addition if
// persistence fails.
bool PlayerStore::add(const std::string& name) {
    if (!isValidPlayerName(name) || count() >= MAX_PLAYERS || contains(name)) return false;
    // Clear any orphaned results before reusing a profile name after an interrupted deletion.
    if (!bestTimes.removePlayer(name)) return false;
    names.push_back(name);
    if (save()) return true;
    names.pop_back();
    return false;
}

// Save the remaining profiles before deleting the selected player's database rows; restore profiles on failure.
bool PlayerStore::remove(int i) {
    if (i < 0 || i >= count()) return false;
    const auto previousNames = names;
    const std::string removedName = names[i];
    names.erase(names.begin() + i);
    if (!save()) {
        names = previousNames;
        return false;
    }
    if (bestTimes.removePlayer(removedName)) return true;
    names = previousNames;
    save();
    return false;
}

// Read the current SQLite result so different store instances do not overwrite stale cached scores.
float PlayerStore::bestTime(const std::string& player, int mazeSize, GameMode mode) const {
    if (!contains(player)) return -1.0f;
    return bestTimes.bestTime(player, mazeSize, mode);
}

// Validate the profile and let the database transaction accept only a first or faster run.
bool PlayerStore::recordBestTime(const std::string& player, int mazeSize, float seconds, GameMode mode) {
    if (!contains(player)) return false;
    return bestTimes.record(player, mazeSize, seconds, mode);
}

// Expose storage errors independently of the boolean indicating that a personal best improved.
bool PlayerStore::bestTimeStorageFailed() const { return bestTimes.storageFailed(); }

// Recover a missing profile file from backup, then load unique valid names up to the configured
// profile limit.
void PlayerStore::load() {
    recoverBackup(FILE_NAME);
    std::ifstream in(FILE_NAME);
    std::string line;
    while (std::getline(in, line) && count() < MAX_PLAYERS) {
        if (isValidPlayerName(line) && !contains(line)) names.push_back(line);
    }
}

// Persist the profile list as one name per line through the temporary-file and backup replacement
// helper.
bool PlayerStore::save() const {
    return writeAtomically(FILE_NAME, [&](std::ostream& out) {
        for (const std::string& n : names) out << n << '\n';
        });
}
