#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace Storage {
struct Connection;
}

// Create the storage schema and import legacy profiles and scores inside one immediate
// transaction. Publish the profile list and import markers only on commit, so an interrupted or
// failed migration can be retried.
bool MigrateDatabase(Storage::Connection& CurrentConnection, const std::filesystem::path& Directory,
                     std::vector<std::string>& CurrentPlayers);
