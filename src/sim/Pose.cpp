#include "sim/Pose.hpp"

#include "sim/Session.hpp"

namespace hr {

VehiclePose restPose(const VehicleDef& def, b2Vec2 position, float angle)
{
    VehiclePose pose;
    pose.chassis = {position, b2MakeRot(angle)};
    for (const WheelDef& w : def.wheels) pose.wheels.push_back({b2TransformPoint(pose.chassis, w.anchor), pose.chassis.q});
    pose.head = {b2TransformPoint(pose.chassis, b2Add(def.neckAnchor, {0.0f, def.headLift})), pose.chassis.q};
    return pose;
}

VehiclePose lerpPose(const VehiclePose& a, const VehiclePose& b, float t)
{
    auto lerpXf = [t](b2Transform x, b2Transform y) {
        return b2Transform{b2Lerp(x.p, y.p, t), b2NLerp(x.q, y.q, t)};
    };
    VehiclePose out = b;
    out.chassis = lerpXf(a.chassis, b.chassis);
    out.head = lerpXf(a.head, b.head);
    for (size_t i = 0; i < out.wheels.size() && i < a.wheels.size(); ++i) out.wheels[i] = lerpXf(a.wheels[i], b.wheels[i]);
    return out;
}

VehiclePose poseOf(const Session& session)
{
    const Vehicle& v = session.vehicle();
    VehiclePose pose;
    pose.chassis = v.chassisTransform();
    for (int i = 0; i < v.wheelCount(); ++i) pose.wheels.push_back(v.wheelTransform(i));
    pose.head = v.headTransform();
    pose.crashed = session.state() == RunState::Crashed;
    return pose;
}

} // namespace hr
