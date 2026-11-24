#include "sim/Pickups.hpp"

#include "core/Math.hpp"
#include "core/Rng.hpp"
#include "sim/Terrain.hpp"

#include <algorithm>
#include <cmath>

namespace hr {

namespace {

constexpr float kChunkWidth = 64.0f;
constexpr float kCoinLift = 1.2f;
constexpr float kFuelLift = 0.95f;
constexpr float kFuelClearance = 3.5f;  // keep coins this far from a fuel can

uint64_t makeId(int chunk, int item)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(chunk)) << 32) | static_cast<uint32_t>(item);
}

} // namespace

Pickups::Pickups(const StageDef& stage, const Terrain& terrain) : stage_(&stage), terrain_(&terrain) {}

float Pickups::fuelPosition(const StageDef& stage, int index)
{
    const float n = static_cast<float>(index);
    return stage.fuelFirst + n * stage.fuelGap + stage.fuelGapGrowth * n * (n - 1.0f) * 0.5f;
}

float Pickups::nextFuelX(float x) const
{
    for (int i = 0; i < 100000; ++i) {
        const float fx = fuelPosition(*stage_, i);
        if (fx >= x) return fx;
    }
    return 1e9f;
}

int Pickups::clusterValue(const StageDef& stage, float x)
{
    const float raw = std::fmin(static_cast<float>(kMaxClusterValue), stage.coinRate * std::fmax(0.0f, x));
    const int value = static_cast<int>(raw / 5.0f) * 5;
    return std::max(5, value);
}

std::vector<int> Pickups::splitCoins(int value)
{
    std::vector<int> coins;
    for (int denomination : {500, 100, 25, 5}) {
        while (value >= denomination) {
            coins.push_back(denomination);
            value -= denomination;
        }
    }
    return coins;
}

float Pickups::coinRadius(int value)
{
    switch (value) {
    case 5: return 0.36f;
    case 25: return 0.40f;
    case 100: return 0.44f;
    default: return 0.50f;
    }
}

void Pickups::ensureRange(float minX, float maxX)
{
    const int first = static_cast<int>(std::floor(minX / kChunkWidth));
    const int last = static_cast<int>(std::floor(maxX / kChunkWidth));
    for (int i = first; i <= last; ++i) {
        if (chunks_.find(i) == chunks_.end()) generateChunk(i);
    }
    for (auto it = chunks_.begin(); it != chunks_.end();) {
        if (it->first < first - 1 || it->first > last + 1) it = chunks_.erase(it);
        else ++it;
    }
}

void Pickups::generateChunk(int index)
{
    std::vector<Pickup>& items = chunks_[index];
    const float x0 = static_cast<float>(index) * kChunkWidth;
    const float x1 = x0 + kChunkWidth;
    int item = 0;

    auto add = [&](PickupKind kind, b2Vec2 pos, int value) {
        const uint64_t id = makeId(index, item++);
        items.push_back({kind, pos, value, id, collected_.count(id) > 0});
    };

    auto deckY = [&](float x) {
        return std::fmax(terrain_->surfaceHeight(x), terrain_->bridgeDeckHeight(x));
    };

    // Fuel cans in (and just around) this chunk; neighbours matter for clearance.
    std::vector<float> fuelNear;
    for (int i = 0;; ++i) {
        const float fx = fuelPosition(*stage_, i);
        if (fx >= x1 + 20.0f) break;
        if (fx >= x0 - 20.0f) fuelNear.push_back(fx);
        if (fx >= x0 && fx < x1) {
            float fy = deckY(fx) + kFuelLift;
            if (terrain_->generator().hasCeiling()) fy = std::fmin(fy, terrain_->ceilingHeight(fx) - 1.2f);
            add(PickupKind::Fuel, {fx, fy}, 0);
        }
    }

    // Coin clusters are centred on 45, 75, 105 ... m: between the 90 m fuel cans.
    const int firstCluster = std::max(1, static_cast<int>(std::ceil((x0 - 15.0f) / kClusterSpacing)));
    for (int k = firstCluster;; ++k) {
        float centre = kClusterSpacing * static_cast<float>(k) + 15.0f;
        if (centre >= x1) break;
        if (centre < x0) continue;

        Rng rng(hashCombine(stage_->seed ^ 0xC01Eull, static_cast<uint64_t>(k)));
        const std::vector<int> coins = splitCoins(clusterValue(*stage_, centre));
        float spacing = 0.0f;
        for (int c : coins) spacing = std::fmax(spacing, coinRadius(c) * 2.0f + 0.22f);
        const float span = static_cast<float>(coins.size() - 1) * spacing;

        // Slide the cluster off any fuel can it overlaps.
        for (float fx : fuelNear) {
            const float reach = span * 0.5f + kFuelClearance;
            if (std::fabs(centre - fx) < reach) centre = centre < fx ? fx - reach : fx + reach;
        }

        const bool arc = coins.size() >= 5 && rng.chance(0.35f);
        const float start = centre - span * 0.5f;
        for (size_t i = 0; i < coins.size(); ++i) {
            const float x = start + static_cast<float>(i) * spacing;
            float lift = kCoinLift + coinRadius(coins[i]) - 0.4f;
            if (arc) {
                const float t = coins.size() > 1 ? static_cast<float>(i) / static_cast<float>(coins.size() - 1) : 0.5f;
                lift += 1.5f * std::sin(t * kPi);
            }
            float cy = deckY(x) + lift;
            if (terrain_->generator().hasCeiling()) cy = std::fmin(cy, terrain_->ceilingHeight(x) - 1.0f);
            add(PickupKind::Coin, {x, cy}, coins[i]);
        }
    }
}

void Pickups::collect(const std::vector<Vehicle::Probe>& probes, std::vector<Pickup>& out)
{
    if (probes.empty()) return;
    float minX = probes[0].centre.x;
    float maxX = minX;
    for (const auto& p : probes) {
        minX = std::fmin(minX, p.centre.x - p.radius);
        maxX = std::fmax(maxX, p.centre.x + p.radius);
    }
    minX -= kFuelRadius;
    maxX += kFuelRadius;

    for (auto& [chunk, items] : chunks_) {
        for (Pickup& pickup : items) {
            if (pickup.collected || pickup.pos.x < minX || pickup.pos.x > maxX) continue;
            const float r = pickup.kind == PickupKind::Fuel ? kFuelRadius : coinRadius(pickup.value);
            for (const auto& probe : probes) {
                const float reach = r + probe.radius;
                if (b2DistanceSquared(probe.centre, pickup.pos) <= reach * reach) {
                    pickup.collected = true;
                    collected_.insert(pickup.id);
                    out.push_back(pickup);
                    break;
                }
            }
        }
    }
}

} // namespace hr
