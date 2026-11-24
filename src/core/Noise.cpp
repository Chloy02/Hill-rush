#include "core/Noise.hpp"

#include "core/Rng.hpp"

#include <cmath>

namespace hr {

float Noise1D::gradient(int64_t cell) const
{
    const uint64_t h = hashCombine(seed_, static_cast<uint64_t>(cell));
    return hashToUnit(h) * 2.0f - 1.0f;
}

float Noise1D::operator()(float x) const
{
    const float cellStart = std::floor(x);
    const int64_t cell = static_cast<int64_t>(cellStart);
    const float t = x - cellStart;

    const float v0 = gradient(cell) * t;
    const float v1 = gradient(cell + 1) * (t - 1.0f);
    const float fade = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    // A 1D Perlin sum lies in [-0.5, 0.5]; scale to [-1, 1].
    return 2.0f * (v0 + (v1 - v0) * fade);
}

float Noise1D::fbm(float x, int octaves, float gain) const
{
    float sum = 0.0f;
    float amplitude = 1.0f;
    float norm = 0.0f;
    float frequency = 1.0f;
    for (int i = 0; i < octaves; ++i) {
        // Offset each octave so their lattice points don't line up.
        sum += amplitude * (*this)(x * frequency + 17.31f * static_cast<float>(i));
        norm += amplitude;
        amplitude *= gain;
        frequency *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

} // namespace hr
