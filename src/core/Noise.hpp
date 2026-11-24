#pragma once

#include <cstdint>

namespace hr {

// 1D gradient (Perlin) noise. Deterministic for a given seed, C2-continuous,
// output in [-1, 1]. Used for terrain and background silhouettes.
class Noise1D {
public:
    explicit Noise1D(uint64_t seed = 0) : seed_(seed) {}

    float operator()(float x) const;

    // Fractal sum of `octaves` layers; each layer doubles frequency and scales
    // amplitude by `gain`. Normalised back to roughly [-1, 1].
    float fbm(float x, int octaves, float gain = 0.5f) const;

private:
    float gradient(int64_t cell) const;

    uint64_t seed_;
};

} // namespace hr
