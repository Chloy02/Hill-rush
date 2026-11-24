#pragma once

#include <algorithm>
#include <cmath>

namespace hr {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;

inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline float smoothstep(float edge0, float edge1, float x)
{
    const float t = clampf((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Wraps an angle into [-pi, pi).
inline float wrapAngle(float a)
{
    a = std::fmod(a + kPi, kTwoPi);
    if (a < 0.0f) a += kTwoPi;
    return a - kPi;
}

// Frame-rate independent exponential approach of `current` towards `target`.
inline float approach(float current, float target, float rate, float dt)
{
    return target + (current - target) * std::exp(-rate * dt);
}

// Moves `current` towards `target` by at most `maxDelta`.
inline float moveTowards(float current, float target, float maxDelta)
{
    if (std::fabs(target - current) <= maxDelta) return target;
    return current + (target > current ? maxDelta : -maxDelta);
}

inline float degToRad(float d) { return d * (kPi / 180.0f); }
inline float radToDeg(float r) { return r * (180.0f / kPi); }

} // namespace hr
