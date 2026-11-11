#pragma once
#include <cstdint>
#include <random>

// Request an unsigned seed from std::random_device for a new non-reproducible run. Callers that
// need repeatable maze layouts or tests should retain a seed and pass it explicitly instead.
inline std::uint32_t RandomSeed() {
    return std::random_device{}();
}
