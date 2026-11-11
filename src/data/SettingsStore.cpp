#include "SettingsStore.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace {
const std::filesystem::path FileName = "save_data/settings.txt";
const std::filesystem::path TEMPORARY = "save_data/settings.txt.tmp";
const std::filesystem::path BACKUP = "save_data/settings.txt.bak";

// Parse one mouse_sensitivity integer record from the requested file, permitting surrounding
// whitespace and rejecting extra content or malformed values. Return an empty optional when the
// file cannot supply a valid record.
std::optional<int> ReadSensitivity(const std::filesystem::path& Path) {
    std::ifstream CurrentInput(Path);
    std::string Key;
    int Value = 0;
    if (!(CurrentInput >> Key >> Value) || Key != "mouse_sensitivity") return std::nullopt;
    CurrentInput >> std::ws;
    if (!CurrentInput.eof()) return std::nullopt;
    return Value;
}

// Best-effort removal of a failed save's temporary file using error_code rather than exceptions.
// Cleanup must not replace the original save failure with a second filesystem exception.
void RemoveTemporary() {
    std::error_code Ignored;
    std::filesystem::remove(TEMPORARY, Ignored);
}
}

// Read the primary settings file, consulting its backup only when the primary is absent. Return
// an empty optional for unavailable or malformed data without creating files or changing
// preferences.
std::optional<int> SettingsStore::LoadMouseSensitivity() const {
    // Read the main file, or its backup if the main file is absent, without changing either file.
    std::error_code Error;
    bool Exists = std::filesystem::exists(FileName, Error);
    if (Error) return std::nullopt;
    return ReadSensitivity(Exists ? FileName : BACKUP);
}

// Write the supplied percentage to a checked temporary file, then replace the settings file using
// a backup for recovery. Return false if saving fails; profiles and scores are never changed by
// this operation.
bool SettingsStore::SaveMouseSensitivity(int Percent) const {
    std::error_code Error;
    std::filesystem::create_directories(FileName.parent_path(), Error);
    if (Error) return false;

    bool HadOriginal = std::filesystem::exists(FileName, Error);
    if (Error) return false;
    if (HadOriginal && !std::filesystem::is_regular_file(FileName, Error)) return false;
    if (Error) return false;

    // Restore an orphaned backup before saving so an interrupted replacement does not discard the last
    // saved value.
    if (!HadOriginal && std::filesystem::exists(BACKUP, Error)) {
        if (Error || !std::filesystem::is_regular_file(BACKUP, Error) || Error) return false;
        std::filesystem::rename(BACKUP, FileName, Error);
        if (Error) return false;
        HadOriginal = true;
    }
    if (Error) return false;

    std::ofstream Output(TEMPORARY, std::ios::binary | std::ios::trunc);
    if (!Output) return false;
    Output << "mouse_sensitivity " << Percent << '\n';
    Output.flush();
    if (!Output) {
        Output.close();
        RemoveTemporary();
        return false;
    }
    Output.close();
    if (!Output) {
        RemoveTemporary();
        return false;
    }

    std::filesystem::remove(BACKUP, Error);
    if (Error) {
        RemoveTemporary();
        return false;
    }
    if (HadOriginal) {
        std::filesystem::rename(FileName, BACKUP, Error);
        if (Error) {
            RemoveTemporary();
            return false;
        }
    }
    std::filesystem::rename(TEMPORARY, FileName, Error);
    if (Error) {
        if (HadOriginal) {
            std::error_code RestoreError;
            std::filesystem::rename(BACKUP, FileName, RestoreError);
        }
        RemoveTemporary();
        return false;
    }
    if (HadOriginal) std::filesystem::remove(BACKUP, Error);
    return true;
}
