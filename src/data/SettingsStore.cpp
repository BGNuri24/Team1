#include "SettingsStore.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace {
const std::filesystem::path FILE_NAME = "save_data/settings.txt";
const std::filesystem::path TEMPORARY = "save_data/settings.txt.tmp";
const std::filesystem::path BACKUP = "save_data/settings.txt.bak";

// Parse exactly one mouse_sensitivity integer record, allowing surrounding whitespace and rejecting
// unexpected content.
std::optional<int> readSensitivity(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::string key;
    int value = 0;
    if (!(input >> key >> value) || key != "mouse_sensitivity") return std::nullopt;
    input >> std::ws;
    if (!input.eof()) return std::nullopt;
    return value;
}

// Attempt to delete the temporary settings file without throwing while cleaning up a failed save.
void removeTemporary() {
    std::error_code ignored;
    std::filesystem::remove(TEMPORARY, ignored);
}
}

// Read the preference without side effects, falling back to the backup only when the primary file is
// missing.
std::optional<int> SettingsStore::loadMouseSensitivity() const {
    // Read the main file, or its backup if the main file is absent, without changing either file.
    std::error_code error;
    bool exists = std::filesystem::exists(FILE_NAME, error);
    if (error) return std::nullopt;
    return readSensitivity(exists ? FILE_NAME : BACKUP);
}

// Write the preference through a checked temporary file and backup replacement, returning false when
// the save cannot complete.
bool SettingsStore::saveMouseSensitivity(int percent) const {
    std::error_code error;
    std::filesystem::create_directories(FILE_NAME.parent_path(), error);
    if (error) return false;

    bool hadOriginal = std::filesystem::exists(FILE_NAME, error);
    if (error) return false;
    if (hadOriginal && !std::filesystem::is_regular_file(FILE_NAME, error)) return false;
    if (error) return false;

    // Restore an orphaned backup before saving so an interrupted replacement does not discard the last
    // saved value.
    if (!hadOriginal && std::filesystem::exists(BACKUP, error)) {
        if (error || !std::filesystem::is_regular_file(BACKUP, error) || error) return false;
        std::filesystem::rename(BACKUP, FILE_NAME, error);
        if (error) return false;
        hadOriginal = true;
    }
    if (error) return false;

    std::ofstream output(TEMPORARY, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output << "mouse_sensitivity " << percent << '\n';
    output.flush();
    if (!output) {
        output.close();
        removeTemporary();
        return false;
    }
    output.close();
    if (!output) {
        removeTemporary();
        return false;
    }

    std::filesystem::remove(BACKUP, error);
    if (error) {
        removeTemporary();
        return false;
    }
    if (hadOriginal) {
        std::filesystem::rename(FILE_NAME, BACKUP, error);
        if (error) {
            removeTemporary();
            return false;
        }
    }
    std::filesystem::rename(TEMPORARY, FILE_NAME, error);
    if (error) {
        if (hadOriginal) {
            std::error_code restoreError;
            std::filesystem::rename(BACKUP, FILE_NAME, restoreError);
        }
        removeTemporary();
        return false;
    }
    if (hadOriginal) std::filesystem::remove(BACKUP, error);
    return true;
}
