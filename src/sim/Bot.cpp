#include "sim/Bot.hpp"

#include "core/Math.hpp"

#include <cmath>

namespace hr {

InputState Bot::think(const Session& session)
{
    const Vehicle& v = session.vehicle();
    const Terrain& terrain = session.terrain();
    const float dt = kFixedDt;
    InputState in;

    const float angle = v.angle();
    const float spin = v.angularVelocity();
    const b2Vec2 vel = v.velocity();
    const float x = v.position().x;

    if (v.airborne()) {
        const float height = v.position().y - terrain.surfaceHeight(x);
        if (stunts_ && height > 4.0f && vel.y > -2.0f) {
            in.gas = 1.0f;  // keep rotating backwards while there is room
        } else {
            // Land parallel to the ground where we are about to touch down.
            const float target = terrain.surfaceAngle(x + vel.x * 0.5f);
            const float error = wrapAngle(angle - target) + spin * 0.35f;
            if (error > 0.10f) in.brake = 1.0f;
            else if (error < -0.10f) in.gas = 1.0f;
        }
    } else {
        // On the ground: full throttle unless the nose is lifting too far.
        const float slope = terrain.surfaceAngle(x);
        const float lift = wrapAngle(angle - slope) + spin * 0.3f;
        if (lift > 0.85f) in.brake = 1.0f;
        else if (lift < 0.5f) in.gas = 1.0f;

        // Lift off before sharp crests: above sqrt(g * R) the car would take off.
        const float speed = v.speed();
        if (vel.x > 3.0f && !stunts_) {
            const float g = session.stage().gravity;
            for (float ahead = 2.0f; ahead <= std::fmin(30.0f, speed * 1.2f + 4.0f); ahead += 2.0f) {
                const float a0 = terrain.surfaceAngle(x + ahead - 2.0f);
                const float a1 = terrain.surfaceAngle(x + ahead + 2.0f);
                const float curvature = (a1 - a0) / 4.0f;  // negative over a crest
                if (curvature >= -0.02f) continue;
                const float launchSpeed = std::sqrt(g / -curvature);
                if (speed > launchSpeed * 1.4f) {
                    in.gas = 0.0f;  // lift off; braking would kill the momentum for the next climb
                    break;
                }
            }
        }
    }

    // Stalled on a climb: roll back down for a run-up, like a human would,
    // then charge the hill again.
    if (retreatTimer_ <= 0.0f) {
        if (v.speed() < 0.5f && in.gas > 0.0f && !v.airborne()) stuckTimer_ += dt;
        else stuckTimer_ = 0.0f;
        if (stuckTimer_ > 1.5f) {
            stuckTimer_ = 0.0f;
            retreatTimer_ = 6.0f;
            retreatFromX_ = x;
        }
    }
    if (retreatTimer_ > 0.0f) {
        retreatTimer_ -= dt;
        const bool farEnough = retreatFromX_ - x > 14.0f;
        const bool inValley = terrain.surfaceAngle(x) > -0.05f && terrain.surfaceAngle(x) < 0.08f && retreatFromX_ - x > 3.0f;
        if (farEnough || inValley) {
            retreatTimer_ = 0.0f;
        } else {
            in.gas = 0.0f;
            // Let gravity do most of the work; reverse gently on gentle ground.
            in.brake = terrain.surfaceAngle(x) < 0.25f ? 1.0f : 0.0f;
            const float lift = wrapAngle(angle - terrain.surfaceAngle(x));
            if (lift < -0.6f) in.brake = 0.0f;  // don't tip over backwards while reversing
        }
    }
    return in;
}

} // namespace hr
