#pragma once

#include "sim/Defs.hpp"

#include <box2d/box2d.h>

#include <vector>

namespace hr {

// A vehicle instance in a Box2D world: chassis, sprung wheels with motors and
// a driver head on a sprung neck.
class Vehicle {
public:
    Vehicle(b2WorldId world, const VehicleDef& def, const VehicleTuning& tuning, b2Vec2 position);
    ~Vehicle();
    Vehicle(const Vehicle&) = delete;
    Vehicle& operator=(const Vehicle&) = delete;

    // Pedals in [0, 1]. Call once per fixed step, before b2World_Step.
    // `engineOn` is false once the fuel has run out (brakes still work).
    void applyControls(float gas, float brake, bool engineOn, float dt);

    // Refreshes ground-contact flags. Call once per fixed step, after b2World_Step.
    void updateContacts();

    // Rotates every part of the vehicle by `angle` around the chassis origin,
    // then moves it by `offset`, and stops all motion. Joints stay satisfied
    // because the whole assembly moves rigidly.
    void rigidMove(float angle, b2Vec2 offset);

    bool headTouchingGround() const { return headTouching_; }
    bool chassisTouchingGround() const { return chassisTouching_; }
    int wheelsOnGround() const;
    bool wheelOnGround(int i) const { return wheels_[static_cast<size_t>(i)].onGround; }
    bool airborne() const { return wheelsOnGround() == 0 && !chassisTouching_; }

    b2Vec2 position() const;
    b2Vec2 velocity() const;
    float angle() const;
    float angularVelocity() const;
    float speed() const;

    // Driven-wheel spin as a fraction of the engine's top wheel speed (0..~1.2).
    float wheelSpinRatio() const;

    // Pedal-driven rotation torque applied in the last step (for effects/tests).
    float lastAirTorque() const { return lastAirTorque_; }
    // Rotational inertia of the whole assembly about the chassis centre of mass.
    float systemInertia() const { return systemInertia_; }

    b2Transform chassisTransform() const;
    b2Transform wheelTransform(int i) const;
    b2Transform headTransform() const;
    // World position of the wheel's suspension mount on the chassis.
    b2Vec2 wheelMountWorld(int i) const;
    int wheelCount() const { return static_cast<int>(wheels_.size()); }
    float wheelRadius(int i) const { return wheels_[static_cast<size_t>(i)].radius; }
    float wheelAngularVelocity(int i) const;
    bool wheelDriven(int i) const { return wheels_[static_cast<size_t>(i)].driveShare > 0.0f; }

    // Circles used for pickup collection: chassis, wheels and head.
    struct Probe {
        b2Vec2 centre;
        float radius;
    };
    void probes(std::vector<Probe>& out) const;

    const VehicleDef& def() const { return *def_; }
    const VehicleTuning& tuning() const { return tuning_; }
    b2BodyId chassisBody() const { return chassis_; }
    b2BodyId headBody() const { return head_; }

private:
    struct Wheel {
        b2BodyId body;
        b2JointId joint;
        b2Vec2 mount;  // chassis space
        float radius;
        float driveShare;
        bool onGround;
    };

    static bool bodyTouching(b2BodyId body);

    const VehicleDef* def_;
    VehicleTuning tuning_;
    b2WorldId world_;
    b2BodyId chassis_{};
    b2BodyId head_{};
    b2JointId neck_{};
    std::vector<Wheel> wheels_;
    float probeRadius_ = 1.0f;
    float systemInertia_ = 1.0f;
    bool headTouching_ = false;
    bool chassisTouching_ = false;
    float lastAirTorque_ = 0.0f;
};

} // namespace hr
