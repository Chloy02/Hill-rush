#include "sim/Terrain.hpp"

#include "core/Math.hpp"
#include "core/Rng.hpp"

#include <cmath>
#include <vector>

namespace hr {

namespace {

constexpr float kCraterCell = 70.0f;

} // namespace

TerrainGenerator::TerrainGenerator(const StageDef& stage)
    : stage_(&stage),
      layers_{Noise1D(hashCombine(stage.seed, 1)), Noise1D(hashCombine(stage.seed, 2)),
              Noise1D(hashCombine(stage.seed, 3))},
      iceNoise_(hashCombine(stage.seed, 4)),
      bumpNoise_(hashCombine(stage.seed, 5))
{
}

float TerrainGenerator::ramp(float x) const
{
    // Ease-out ramp: difficulty climbs quickly at first, then levels off.
    const float progress = clampf(std::fmax(0.0f, x) / stage_->rampDistance, 0.0f, 1.0f);
    return 1.0f - (1.0f - progress) * (1.0f - progress);
}

float TerrainGenerator::sample(int index) const
{
    if (index <= 0) return rawHeight(static_cast<float>(index) * kSampleStep);
    if (samples_.empty()) samples_.push_back(rawHeight(0.0f));
    while (static_cast<int>(samples_.size()) <= index) {
        const int i = static_cast<int>(samples_.size());
        const float x = static_cast<float>(i) * kSampleStep;
        const float maxDeg = lerpf(stage_->maxSlopeStart, stage_->maxSlopeEnd, ramp(x));
        const float maxRise = std::tan(degToRad(maxDeg)) * kSampleStep;
        const float previous = samples_.back();
        samples_.push_back(clampf(rawHeight(x), previous - maxRise, previous + maxRise));
    }
    return samples_[static_cast<size_t>(index)];
}

float TerrainGenerator::height(float x) const
{
    if (x <= 0.0f) return rawHeight(x);
    const float f = x / kSampleStep;
    const float cell = std::floor(f);
    const int i = static_cast<int>(cell);
    return lerpf(sample(i), sample(i + 1), f - cell);
}

float TerrainGenerator::rawHeight(float x) const
{
    if (x < kStartWallX) {
        // Steep wall behind the start so the car can't wander off to the left.
        const float d = kStartWallX - x;
        return d * d * 0.12f + d * 0.8f;
    }
    float h = baseHeight(x);
    if (stage_->extraBumps > 0.0f) h += extraBumps(x);
    return h;
}

float TerrainGenerator::extraBumps(float x) const
{
    // A bump-and-dip right at the foot of a steep climb kills the car's
    // momentum and turns the hill into a dead end. So the extra rough ground
    // lives on flats, descents and gentle rolls, and fades out where the
    // stage's own terrain is steep or about to climb steeply.
    const float here = baseHeight(x);
    const float slopeNow = std::fabs(baseHeight(x + 1.0f) - baseHeight(x - 1.0f)) * 0.5f;
    const float climbAhead = (baseHeight(x + 12.0f) - here) / 12.0f;
    const float steepness = std::fmax(slopeNow, climbAhead);
    const float keep = 1.0f - smoothstep(0.2f, 0.45f, steepness);
    if (keep <= 0.0f) return 0.0f;

    // A fixed real-world size (not the stage's own bumps, which are tiny on
    // smooth stages like the desert), growing as the track gets harder.
    const float amplitude = lerpf(0.5f, 1.4f, ramp(x)) * stage_->extraBumps;
    const float envelope = smoothstep(18.0f, 80.0f, x);
    return amplitude * bumpNoise_(x / 9.0f) * keep * envelope;
}

float TerrainGenerator::baseHeight(float x) const
{
    const float ramp = this->ramp(x);

    float h = 0.0f;
    float totalAmplitude = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const TerrainOctave& o = stage_->octaves[static_cast<size_t>(i)];
        const float amplitude = lerpf(o.ampStart, o.ampEnd, ramp);
        h += amplitude * layers_[i](x / o.wavelength);
        totalAmplitude += amplitude;
    }
    // Steepen: an S-curve on the normalised height keeps peaks and valleys
    // where they are but makes the faces between them steeper.
    const float steep = lerpf(stage_->steepStart, stage_->steepEnd, ramp);
    if (steep > 1.001f && totalAmplitude > 0.0f) {
        // The noise rarely exceeds half its amplitude, so normalise by that.
        const float u = h / (0.5f * totalAmplitude);
        h = 0.5f * totalAmplitude * std::tanh(steep * u) / std::tanh(steep);
    }
    // Flat start pad at height 0, then the hills fade in.
    const float envelope = smoothstep(18.0f, 80.0f, x);
    h *= envelope;

    if (stage_->feature == TerrainFeature::Craters) h += craterOffset(x) * envelope;
    return h;
}

float TerrainGenerator::craterOffset(float x) const
{
    const float cellF = std::floor(x / kCraterCell);
    const int64_t cell = static_cast<int64_t>(cellF);
    Rng rng(hashCombine(stage_->seed ^ 0xC4A7E45ull, static_cast<uint64_t>(cell)));
    if (rng.unit() > 0.6f) return 0.0f;

    const float radius = rng.range(4.5f, 10.0f);
    // Keep the whole crater (including its rim) inside the cell.
    const float margin = radius * 1.4f + 1.0f;
    const float centre = cellF * kCraterCell + rng.range(margin, kCraterCell - margin);
    const float depth = rng.range(0.8f, 2.2f) * smoothstep(0.0f, 1500.0f, x) + 0.6f;

    const float u = std::fabs(x - centre) / radius;
    float offset = 0.0f;
    if (u < 1.0f) {
        const float k = 1.0f - u * u;
        offset -= depth * k * k;
    }
    if (u > 0.75f && u < 1.35f) {
        const float s = (u - 0.75f) / 0.6f;
        const float bump = std::sin(kPi * s);
        offset += 0.35f * depth * bump * bump;
    }
    return offset;
}

bool TerrainGenerator::isIce(float x) const
{
    if (stage_->feature != TerrainFeature::IcePatches || x < 120.0f) return false;
    return iceNoise_(x / 32.0f) > 0.32f;
}

float TerrainGenerator::friction(float x) const
{
    return isIce(x) ? stage_->iceFriction : stage_->groundFriction;
}

// ---------------------------------------------------------------------------

Terrain::Terrain(b2WorldId world, const StageDef& stage)
    : world_(world), generator_(stage)
{
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_staticBody;
    bodyDef.name = "ground";
    groundBody_ = b2CreateBody(world_, &bodyDef);

    // Hard stop far behind the start line.
    b2ShapeDef wallDef = b2DefaultShapeDef();
    wallDef.filter.categoryBits = kCategoryGround;
    const float wallX = TerrainGenerator::kStartWallX - 30.0f;
    b2Polygon wall = b2MakeOffsetBox(1.0f, 200.0f, {wallX, generator_.height(wallX)}, b2Rot_identity);
    b2CreatePolygonShape(groundBody_, &wallDef, &wall);
}

Terrain::~Terrain()
{
    if (b2Body_IsValid(groundBody_)) b2DestroyBody(groundBody_);
}

void Terrain::ensureRange(float minX, float maxX)
{
    const int first = static_cast<int>(std::floor(minX / kChunkWidth));
    const int last = static_cast<int>(std::floor(maxX / kChunkWidth));

    for (int i = first; i <= last; ++i) {
        if (chunks_.find(i) == chunks_.end()) createChunk(i);
    }

    // Keep a margin of one chunk on each side so a car rolling back and forth
    // across a boundary never makes us rebuild the same chunk repeatedly.
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (it->first < first - 1 || it->first > last + 1) {
            b2DestroyChain(it->second);
            it = chunks_.erase(it);
        } else {
            ++it;
        }
    }
}

void Terrain::createChunk(int index)
{
    const int firstSample = index * kSamplesPerChunk;
    const int lastSample = firstSample + kSamplesPerChunk;

    // Box2D chains are one-sided and collide on the right of their direction,
    // so the points run right-to-left to make the solid side face up. The
    // first and last points are ghost vertices borrowed from the neighbouring
    // chunks, which keeps wheels from catching on the seams.
    std::vector<b2Vec2> points;
    std::vector<b2SurfaceMaterial> materials;
    points.reserve(static_cast<size_t>(kSamplesPerChunk + 3));
    materials.reserve(points.capacity());
    for (int s = lastSample + 1; s >= firstSample - 1; --s) {
        points.push_back({sampleX(s), sampleHeight(s)});
        b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
        // Material i belongs to the segment starting at point i (heading left).
        material.friction = generator_.friction(sampleX(s) - kStep * 0.5f);
        material.rollingResistance = generator_.stage().rollingResistance;
        material.userMaterialId = generator_.isIce(sampleX(s) - kStep * 0.5f) ? 1 : 0;
        materials.push_back(material);
    }

    b2ChainDef chainDef = b2DefaultChainDef();
    chainDef.points = points.data();
    chainDef.count = static_cast<int>(points.size());
    chainDef.materials = materials.data();
    chainDef.materialCount = static_cast<int>(materials.size());
    chainDef.filter.categoryBits = kCategoryGround;
    chainDef.isLoop = false;
    chunks_[index] = b2CreateChain(groundBody_, &chainDef);
}

float Terrain::surfaceHeight(float x) const
{
    const float f = std::floor(x / kStep);
    const int i = static_cast<int>(f);
    const float t = x / kStep - f;
    return lerpf(sampleHeight(i), sampleHeight(i + 1), t);
}

float Terrain::surfaceAngle(float x) const
{
    const int i = static_cast<int>(std::floor(x / kStep));
    return std::atan2(sampleHeight(i + 1) - sampleHeight(i), kStep);
}

} // namespace hr
