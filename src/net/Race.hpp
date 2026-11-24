#pragma once

#include "net/Protocol.hpp"

#include <cstddef>
#include <deque>
#include <vector>

namespace hr::net {

// Race order: finishers by finish time, then everyone else by distance,
// players who left at the end. Returns indices into `players`.
std::vector<size_t> standings(const std::vector<Player>& players);

// True once nobody is still driving.
bool nobodyDriving(const std::vector<Player>& players);

// Recent snapshots of one remote car, so it can be drawn smoothly a little
// in the past (between two snapshots) however unevenly packets arrive.
class SnapshotBuffer {
public:
    // Ignores snapshots older than the newest one (packets can arrive out of order).
    void push(const CarSnapshot& s);
    void clear() { items_.clear(); }
    bool empty() const { return items_.empty(); }
    size_t size() const { return items_.size(); }
    const CarSnapshot& latest() const { return items_.back(); }

    // The car at race time `t`: interpolated between the snapshots around
    // it. Past the newest one (a late packet, a slow frame) the car glides on
    // at its last speed for up to kMaxGlide seconds, then waits. False when
    // there is nothing yet.
    bool sample(float t, CarSnapshot& out) const;

    static constexpr float kMaxGlide = 0.25f;

private:
    static constexpr float kKeepSeconds = 2.0f;
    static constexpr size_t kMaxItems = 128;  // however fast snapshots arrive
    std::deque<CarSnapshot> items_;
};

} // namespace hr::net
