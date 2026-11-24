#pragma once

#include "sim/Defs.hpp"
#include "sim/Vehicle.hpp"

#include <box2d/math_functions.h>

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace hr {

class Terrain;

enum class PickupKind { Coin, Fuel };

struct Pickup {
    PickupKind kind;
    b2Vec2 pos;
    int value;      // coin value (0 for fuel)
    uint64_t id;    // stable across chunk rebuilds
    bool collected;
};

// Places coins and fuel cans along the track. Placement depends only on the
// stage, so every run of a stage has the same pickups in the same places.
class Pickups {
public:
    static constexpr float kFuelRadius = 0.62f;
    static constexpr float kClusterSpacing = 30.0f;
    static constexpr int kMaxClusterValue = 7000;

    Pickups(const StageDef& stage, const Terrain& terrain);

    void ensureRange(float minX, float maxX);

    // Collects every pickup touched by one of the probes and appends it to `out`.
    void collect(const std::vector<Vehicle::Probe>& probes, std::vector<Pickup>& out);

    // Calls fn(const Pickup&) for uncollected pickups whose x is in [x0, x1].
    template <typename Fn>
    void forEachVisible(float x0, float x1, Fn&& fn) const
    {
        for (const auto& [chunk, items] : chunks_) {
            for (const Pickup& p : items) {
                if (!p.collected && p.pos.x >= x0 && p.pos.x <= x1) fn(p);
            }
        }
    }

    // X of the first fuel can at or after `x`.
    float nextFuelX(float x) const;

    // Pure placement rules (exposed for tests) ------------------------------
    static float fuelPosition(const StageDef& stage, int index);
    // Total value of the coin cluster centred at `x`.
    static int clusterValue(const StageDef& stage, float x);
    // Splits a value into 500/100/25/5 coins, largest first.
    static std::vector<int> splitCoins(int value);
    static float coinRadius(int value);

private:
    void generateChunk(int index);

    const StageDef* stage_;
    const Terrain* terrain_;
    std::map<int, std::vector<Pickup>> chunks_;
    std::set<uint64_t> collected_;
};

} // namespace hr
