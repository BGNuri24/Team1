#pragma once
#include <cstdint>
#include <random>

// Supply entropy only when a caller has not requested a reproducible run.
inline std::uint32_t randomSeed() {
    return std::random_device{}();
}
