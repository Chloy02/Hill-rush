#pragma once

#include "sim/Defs.hpp"

#include <box2d/math_functions.h>

#include <vector>

namespace hr {

class Session;

// Where every part of a vehicle is, for drawing. Built from the physics
// bodies during a run, from the definition for previews, or from network
// snapshots for other players' cars.
struct VehiclePose {
    b2Transform chassis{};
    std::vector<b2Transform> wheels;
    b2Transform head{};
    bool crashed = false;
    bool rollCageBroken = false;
    int rollCageDamage = 0;
    bool thrusting = false;
    bool boosting = false;
};

VehiclePose restPose(const VehicleDef& def, b2Vec2 position, float angle);
VehiclePose lerpPose(const VehiclePose& a, const VehiclePose& b, float t);
// The session's vehicle as it is right now.
VehiclePose poseOf(const Session& session);

} // namespace hr
