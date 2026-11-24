#pragma once

#include <cstdint>

namespace hr {

// SplitMix64 finaliser: a good 64-bit integer hash.
inline uint64_t hash64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

inline uint64_t hashCombine(uint64_t a, uint64_t b)
{
    return hash64(a ^ (hash64(b) + 0x9E3779B97F4A7C15ull + (a << 6) + (a >> 2)));
}

// Maps a hash to a float in [0, 1).
inline float hashToUnit(uint64_t h)
{
    return static_cast<float>(h >> 40) * (1.0f / 16777216.0f);
}

// Small deterministic generator (SplitMix64). Same seed -> same sequence on every platform.
class Rng {
public:
    explicit Rng(uint64_t seed) : state_(seed) {}

    uint64_t next()
    {
        state_ += 0x9E3779B97F4A7C15ull;
        uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // Uniform float in [0, 1).
    float unit() { return hashToUnit(next()); }

    float range(float lo, float hi) { return lo + (hi - lo) * unit(); }

    // Uniform integer in [lo, hi] (inclusive).
    int rangeInt(int lo, int hi)
    {
        const uint64_t span = static_cast<uint64_t>(hi - lo) + 1u;
        return lo + static_cast<int>(next() % span);
    }

    bool chance(float p) { return unit() < p; }

private:
    uint64_t state_;
};

} // namespace hr
