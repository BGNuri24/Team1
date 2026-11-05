#pragma once
#include <optional>
#include <string>
#include <vector>

// Return an operation's outcome directly instead of relying on mutable state from an earlier call.
enum class StorageStatus { Success, Invalid, NotFound, Duplicate, Full, Unavailable };
enum class RecordResult { Updated, Unchanged, Invalid, StorageError };

struct ProfileResult {
    StorageStatus status = StorageStatus::Unavailable;
    std::vector<std::string> players;
};

struct BestTimeResult {
    StorageStatus status = StorageStatus::Unavailable;
    std::optional<float> seconds;
};
