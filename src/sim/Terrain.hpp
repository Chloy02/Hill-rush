#pragma once

#include "core/Noise.hpp"
#include "sim/Defs.hpp"

#include <box2d/box2d.h>

#include <map>
#include <vector>

namespace hr {

// Collision categories shared by the simulation.
inline constexpr uint64_t kCategoryGround = 0x1;
inline constexpr uint64_t kCategoryVehicle = 0x2;

// Pure, deterministic description of a stage's ground. The same stage always
// produces the same track, so best distances are comparable between runs.
class TerrainGenerator {
public:
    // Ground behind the start line rises into a steep wall at this x.
    static constexpr float kStartWallX = -14.0f;

    // Samples are spaced this far apart; the ground is linear between them.
    static constexpr float kSampleStep = 0.5f;

    explicit TerrainGenerator(const StageDef& stage);

    // Ground height (metres) at world x: the slope-capped sample polyline.
    float height(float x) const;
    float sample(int index) const;

    // True where the ground is ice (Arctic only).
    bool isIce(float x) const;

    float friction(float x) const;

    const StageDef& stage() const { return *stage_; }

private:
    // Smooth analytic profile before slope capping.
    float rawHeight(float x) const;
    // The stage's own hills and bumps (all an Easy stage has).
    float baseHeight(float x) const;
    // Medium/Hard: extra rough ground, faded out where a steep climb starts.
    float extraBumps(float x) const;
    float craterOffset(float x) const;
    float ramp(float x) const;

    const StageDef* stage_;
    Noise1D layers_[3];
    Noise1D iceNoise_;
    Noise1D bumpNoise_;
    // Capped samples for x >= 0, generated in order from the start line so the
    // result is identical every time.
    mutable std::vector<float> samples_;
};

// Streams the ground into the Box2D world as chain shapes, one per 64 m chunk.
// The collision surface is the generator sampled every kStep metres; use
// surfaceHeight() to get exactly what the wheels touch.
class Terrain {
public:
    static constexpr float kStep = TerrainGenerator::kSampleStep;
    static constexpr int kSamplesPerChunk = 128;
    static constexpr float kChunkWidth = kStep * kSamplesPerChunk;

    Terrain(b2WorldId world, const StageDef& stage);
    ~Terrain();
    Terrain(const Terrain&) = delete;
    Terrain& operator=(const Terrain&) = delete;

    // Makes sure every chunk overlapping [minX, maxX] exists and drops chunks
    // that are far outside it.
    void ensureRange(float minX, float maxX);

    static float sampleX(int index) { return static_cast<float>(index) * kStep; }
    float sampleHeight(int index) const { return generator_.sample(index); }

    // Height of the collision polyline at x (linear between samples).
    float surfaceHeight(float x) const;
    // Angle (radians) of the polyline segment under x. Positive = uphill.
    float surfaceAngle(float x) const;

    const TerrainGenerator& generator() const { return generator_; }
    int chunkCount() const { return static_cast<int>(chunks_.size()); }

private:
    void createChunk(int index);

    b2WorldId world_;
    b2BodyId groundBody_;
    TerrainGenerator generator_;
    std::map<int, b2ChainId> chunks_;
};

} // namespace hr
