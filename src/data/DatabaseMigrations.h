#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace storage {
    struct Connection;
}

// Commit schema creation and both legacy imports together, leaving existing scores and text backups
// intact.
bool migrateDatabase(storage::Connection& connection, const std::filesystem::path& directory,
    std::vector<std::string>& players);
