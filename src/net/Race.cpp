#include "net/Race.hpp"

#include "core/Math.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace hr::net {

namespace {

int group(const Player& p)
{
    switch (p.state) {
    case RacerState::Finished: return 0;
    case RacerState::Left: return 2;
    default: return 1;
    }
}

} // namespace

std::vector<size_t> standings(const std::vector<Player>& players)
{
    std::vector<size_t> order(players.size());
    std::iota(order.begin(), order.end(), size_t{0});
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Player& pa = players[a];
        const Player& pb = players[b];
        if (group(pa) != group(pb)) return group(pa) < group(pb);
        if (group(pa) == 0 && pa.finishTime != pb.finishTime) return pa.finishTime < pb.finishTime;
        return pa.distance > pb.distance;
    });
    return order;
}

bool nobodyDriving(const std::vector<Player>& players)
{
    return std::none_of(players.begin(), players.end(), [](const Player& p) { return p.state == RacerState::Driving; });
}

void SnapshotBuffer::push(const CarSnapshot& s)
{
    // A clock far behind the newest snapshot means the sender started over.
    if (!items_.empty() && s.time < items_.back().time - 5.0f) items_.clear();
    if (!items_.empty() && s.time < items_.back().time) return;
    if (!items_.empty() && s.time == items_.back().time) {
        items_.back() = s;
        return;
    }
    items_.push_back(s);
    while (items_.size() > 2 && items_.front().time < s.time - kKeepSeconds) items_.pop_front();
    while (items_.size() > kMaxItems) items_.pop_front();
}

bool SnapshotBuffer::sample(float t, CarSnapshot& out) const
{
    if (items_.empty()) return false;
    if (t <= items_.front().time) {
        out = items_.front();
        return true;
    }
    if (t >= items_.back().time) {
        out = items_.back();
        if (out.state != RacerState::Driving || out.pose.crashed) return true;  // a wreck stays put
        const float ahead = std::fmin(t - out.time, kMaxGlide);
        const b2Vec2 shift = b2MulSV(ahead, out.velocity);
        out.pose.chassis.p = b2Add(out.pose.chassis.p, shift);
        out.pose.head.p = b2Add(out.pose.head.p, shift);
        for (b2Transform& w : out.pose.wheels) w.p = b2Add(w.p, shift);
        return true;
    }
    for (size_t i = 1; i < items_.size(); ++i) {
        const CarSnapshot& b = items_[i];
        if (b.time < t) continue;
        const CarSnapshot& a = items_[i - 1];
        const float span = b.time - a.time;
        const float alpha = span > 1e-6f ? (t - a.time) / span : 1.0f;
        out = b;
        out.time = t;
        out.distance = lerpf(a.distance, b.distance, alpha);
        out.velocity = b2Lerp(a.velocity, b.velocity, alpha);
        // Poses only blend when they describe the same vehicle.
        if (a.pose.wheels.size() == b.pose.wheels.size()) out.pose = lerpPose(a.pose, b.pose, alpha);
        return true;
    }
    out = items_.back();
    return true;
}

} // namespace hr::net
