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
      bumpNoise_(hashCombine(stage.seed, 5)),
      mudNoise_(hashCombine(stage.seed, 6)),
      ceilingNoise_(hashCombine(stage.seed, 7))
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
    if (stage_->feature == TerrainFeature::MudPits) h += creekOffset(x) * envelope;
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

float TerrainGenerator::creekOffset(float x) const
{
    if (x < kBridgeFirst) return 0.0f;
    const float k = std::floor((x - kBridgeFirst) / kBridgeInterval);
    const float rel = x - (kBridgeFirst + k * kBridgeInterval);
    if (rel <= 0.0f || rel >= kBridgeSpan) return 0.0f;
    const float u = rel / kBridgeSpan;
    const float s = std::sin(kPi * u);
    return -1.8f * s * s;
}

bool TerrainGenerator::isIce(float x) const
{
    if (stage_->feature != TerrainFeature::IcePatches || x < 120.0f) return false;
    return iceNoise_(x / 32.0f) > 0.32f;
}

bool TerrainGenerator::isMud(float x) const
{
    if (stage_->feature != TerrainFeature::MudPits || x <= 80.0f) return false;
    return mudNoise_(x / 26.0f) > 0.28f;
}

float TerrainGenerator::ceilingHeight(float x) const
{
    if (!hasCeiling()) return 1e6f;
    float localMax = height(x);
    for (float dx = -2.5f; dx <= 2.5f; dx += 0.5f) {
        localMax = std::fmax(localMax, height(x + dx));
    }
    float clearance = lerpf(stage_->ceilingBase, stage_->ceilingMin + 1.4f, ramp(x)) +
                      ceilingNoise_(x / 20.0f) * 1.3f;
    const float startOpen = 1.0f - smoothstep(0.0f, 30.0f, x);
    clearance = lerpf(clearance, 14.0f, startOpen);
    clearance = std::fmax(clearance, stage_->ceilingMin);
    return localMax + clearance;
}

float TerrainGenerator::friction(float x) const
{
    if (isIce(x)) return stage_->iceFriction;
    if (isMud(x)) return stage_->mudFriction;
    return stage_->groundFriction;
}

float TerrainGenerator::rollingResistance(float x) const
{
    return isMud(x) ? stage_->mudRollingResistance : stage_->rollingResistance;
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
    for (const auto& [chunk, list] : bridges_) {
        (void)chunk;
        for (const Bridge& b : list) {
            for (const BridgePlank& p : b.planks) {
                if (b2Body_IsValid(p.body)) b2DestroyBody(p.body);
            }
        }
    }
    bridges_.clear();
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
    for (auto it = ceilingChunks_.begin(); it != ceilingChunks_.end();) {
        if (it->first < first - 1 || it->first > last + 1) {
            b2DestroyChain(it->second);
            it = ceilingChunks_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = bridges_.begin(); it != bridges_.end();) {
        if (it->first < first - 1 || it->first > last + 1) {
            for (const Bridge& b : it->second) {
                for (const BridgePlank& p : b.planks) {
                    if (b2Body_IsValid(p.body)) b2DestroyBody(p.body);
                }
            }
            it = bridges_.erase(it);
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
        const float midX = sampleX(s) - kStep * 0.5f;
        b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
        // Material i belongs to the segment starting at point i (heading left).
        material.friction = generator_.friction(midX);
        material.rollingResistance = generator_.rollingResistance(midX);
        material.userMaterialId = generator_.isIce(midX) ? 1 : (generator_.isMud(midX) ? 2 : 0);
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

    if (generator_.hasCeiling()) {
        // Points run left-to-right so the one-sided chain's solid side faces downwards.
        std::vector<b2Vec2> cPoints;
        std::vector<b2SurfaceMaterial> cMaterials;
        cPoints.reserve(static_cast<size_t>(kSamplesPerChunk + 3));
        cMaterials.reserve(cPoints.capacity());
        for (int s = firstSample - 1; s <= lastSample + 1; ++s) {
            const float x = sampleX(s);
            cPoints.push_back({x, generator_.ceilingHeight(x)});
            b2SurfaceMaterial mat = b2DefaultSurfaceMaterial();
            mat.friction = generator_.stage().groundFriction;
            cMaterials.push_back(mat);
        }
        b2ChainDef cDef = b2DefaultChainDef();
        cDef.points = cPoints.data();
        cDef.count = static_cast<int>(cPoints.size());
        cDef.materials = cMaterials.data();
        cDef.materialCount = static_cast<int>(cMaterials.size());
        cDef.filter.categoryBits = kCategoryGround;
        cDef.isLoop = false;
        ceilingChunks_[index] = b2CreateChain(groundBody_, &cDef);
    }

    if (generator_.stage().feature == TerrainFeature::MudPits) {
        const float cx0 = static_cast<float>(index) * kChunkWidth;
        const float cx1 = cx0 + kChunkWidth;
        if (cx1 > TerrainGenerator::kBridgeFirst) {
            const float startK = std::max(0.0f, std::ceil((cx0 - TerrainGenerator::kBridgeFirst) /
                                                          TerrainGenerator::kBridgeInterval));
            for (float bx0 = TerrainGenerator::kBridgeFirst + startK * TerrainGenerator::kBridgeInterval;
                 bx0 < cx1; bx0 += TerrainGenerator::kBridgeInterval) {
                createBridge(index, bx0);
            }
        }
    }
}

void Terrain::createBridge(int chunkIndex, float bx0)
{
    const float bx1 = bx0 + TerrainGenerator::kBridgeSpan;
    const float y0 = surfaceHeight(bx0);
    const float y1 = surfaceHeight(bx1);
    constexpr int kPlanks = 8;
    constexpr float kHalfH = 0.14f;

    const b2Vec2 p0{bx0, y0 - kHalfH};
    const b2Vec2 p1{bx1, y1 - kHalfH};
    const b2Vec2 diff = b2Sub(p1, p0);
    const float span = b2Length(diff);
    const b2Vec2 dir = b2MulSV(1.0f / span, diff);
    const float theta = std::atan2(diff.y, diff.x);
    const b2Rot rot = b2MakeRot(theta);
    const float segLen = span / static_cast<float>(kPlanks);
    const float halfW = segLen * 0.52f;

    Bridge bridge{bx0, bx1, y0, y1, {}};
    bridge.planks.reserve(kPlanks);
    for (int i = 0; i < kPlanks; ++i) {
        b2BodyDef plankDef = b2DefaultBodyDef();
        plankDef.type = b2_dynamicBody;
        plankDef.position = b2MulAdd(p0, (static_cast<float>(i) + 0.5f) * segLen, dir);
        plankDef.rotation = rot;
        plankDef.linearDamping = 0.25f;
        plankDef.angularDamping = 0.8f;
        plankDef.enableSleep = false;
        plankDef.name = "plank";
        const b2BodyId body = b2CreateBody(world_, &plankDef);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.density = 18.0f;
        shapeDef.material.friction = 0.9f;
        shapeDef.filter.categoryBits = kCategoryGround;
        shapeDef.filter.maskBits = kCategoryVehicle;
        const b2Polygon box = b2MakeBox(halfW, kHalfH);
        b2CreatePolygonShape(body, &shapeDef, &box);
        bridge.planks.push_back({body, halfW, kHalfH});
    }

    auto makeJoint = [&](b2BodyId a, b2Vec2 anchorA, b2BodyId b, b2Vec2 anchorB, float refAngle) {
        b2RevoluteJointDef jd = b2DefaultRevoluteJointDef();
        jd.bodyIdA = a;
        jd.bodyIdB = b;
        jd.localAnchorA = anchorA;
        jd.localAnchorB = anchorB;
        jd.referenceAngle = refAngle;
        jd.enableLimit = true;
        jd.lowerAngle = -0.22f;
        jd.upperAngle = 0.22f;
        jd.enableSpring = true;
        jd.hertz = 3.5f;
        jd.dampingRatio = 0.7f;
        b2CreateRevoluteJoint(world_, &jd);
    };

    makeJoint(groundBody_, p0, bridge.planks.front().body, {-segLen * 0.5f, 0.0f}, theta);
    for (int i = 0; i + 1 < kPlanks; ++i) {
        makeJoint(bridge.planks[static_cast<size_t>(i)].body, {segLen * 0.5f, 0.0f},
                  bridge.planks[static_cast<size_t>(i + 1)].body, {-segLen * 0.5f, 0.0f}, 0.0f);
    }
    makeJoint(bridge.planks.back().body, {segLen * 0.5f, 0.0f}, groundBody_, p1, -theta);
    bridges_[chunkIndex].push_back(std::move(bridge));
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

float Terrain::ceilingHeight(float x) const
{
    if (!generator_.hasCeiling()) return 1e6f;
    const float f = std::floor(x / kStep);
    const int i = static_cast<int>(f);
    const float t = x / kStep - f;
    return lerpf(generator_.ceilingHeight(sampleX(i)), generator_.ceilingHeight(sampleX(i + 1)), t);
}

float Terrain::bridgeDeckHeight(float x) const
{
    if (generator_.stage().feature != TerrainFeature::MudPits || x < TerrainGenerator::kBridgeFirst) return -1e6f;
    const float k = std::floor((x - TerrainGenerator::kBridgeFirst) / TerrainGenerator::kBridgeInterval);
    const float bx0 = TerrainGenerator::kBridgeFirst + k * TerrainGenerator::kBridgeInterval;
    const float bx1 = bx0 + TerrainGenerator::kBridgeSpan;
    if (x < bx0 || x > bx1) return -1e6f;
    return lerpf(surfaceHeight(bx0), surfaceHeight(bx1), (x - bx0) / TerrainGenerator::kBridgeSpan);
}

} // namespace hr
