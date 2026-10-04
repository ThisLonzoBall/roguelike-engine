#pragma once

#include <cstdint>

// Small seeded PRNG (xorshift32). Simulation code draws from an Rng it owns
// rather than a global source, so the same seed and inputs always produce the
// same run.
struct Rng {
    uint32_t state = 0x9E3779B9u;  // must be non-zero

    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Uniform in [0, 1).
    float unit() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    // Uniform in [lo, hi).
    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }
};
