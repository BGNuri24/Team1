#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace storage {
struct Connection;
}

// Create the storage schema and import legacy profiles and scores inside one immediate
// transaction. Publish the profile list and import markers only on commit, so an interrupted or
// failed migration can be retried.
bool migrateDatabase(storage::Connection& connection, const std::filesystem::path& directory,
                     std::vector<std::string>& players);
