#include "sim/Vehicle.hpp"

#include "core/Math.hpp"
#include "sim/Terrain.hpp"

#include <cmath>

namespace hr {

namespace {

constexpr float kHeadMass = 5.0f;
constexpr float kNeckHertz = 5.0f;
constexpr float kNeckDamping = 0.35f;
constexpr float kNeckLimit = 0.45f;       // radians either way
constexpr float kCoastTorque = 6.0f;      // N*m of drag on undriven / released wheels

b2ShapeDef vehicleShapeDef()
{
    b2ShapeDef def = b2DefaultShapeDef();
    def.filter.categoryBits = kCategoryVehicle;
    def.filter.maskBits = kCategoryGround;  // car parts never collide with each other
    return def;
}

} // namespace

Vehicle::Vehicle(b2WorldId world, const VehicleDef& def, const VehicleTuning& tuning, b2Vec2 position)
    : def_(&def), tuning_(tuning), world_(world)
{
    // Chassis -----------------------------------------------------------------
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = position;
    bodyDef.name = "chassis";
    bodyDef.enableSleep = false;
    chassis_ = b2CreateBody(world_, &bodyDef);

    float extent = 0.0f;
    for (const ConvexPart& part : def.chassis) {
        b2Hull hull = b2ComputeHull(part.points.data(), static_cast<int>(part.points.size()));
        b2Polygon polygon = b2MakePolygon(&hull, 0.02f);
        b2ShapeDef shapeDef = vehicleShapeDef();
        shapeDef.density = part.density;
        shapeDef.material.friction = 0.6f;
        b2CreatePolygonShape(chassis_, &shapeDef, &polygon);
        for (const b2Vec2& p : part.points) extent = std::fmax(extent, b2Length(p));
    }
    probeRadius_ = extent * 0.55f;

    if (!def.rollCage.empty() && tuning_.rollCageHits > 0) {
        rollCageMaxHits_ = tuning_.rollCageHits;
        rollCageHitsLeft_ = tuning_.rollCageHits;
        for (const ConvexPart& part : def.rollCage) {
            b2Hull hull = b2ComputeHull(part.points.data(), static_cast<int>(part.points.size()));
            b2Polygon polygon = b2MakePolygon(&hull, 0.02f);
            b2ShapeDef shapeDef = vehicleShapeDef();
            shapeDef.density = part.density;
            shapeDef.material.friction = 0.45f;
            rollCageShapes_.push_back(b2CreatePolygonShape(chassis_, &shapeDef, &polygon));
        }
    }

    // Wheels ------------------------------------------------------------------
    for (const WheelDef& wd : def.wheels) {
        Wheel wheel{};
        wheel.mount = wd.anchor;
        wheel.radius = wd.radius;
        // Engine torque is split across driven wheels: 4WD shares it with the
        // front instead of adding more, so it helps grip without extra wheelies.
        wheel.driveShare = wd.driven ? 1.0f : tuning_.frontDriveShare;

        b2BodyDef wheelBody = b2DefaultBodyDef();
        wheelBody.type = b2_dynamicBody;
        wheelBody.position = b2Add(position, wd.anchor);
        wheelBody.allowFastRotation = true;
        wheelBody.enableSleep = false;
        wheelBody.name = "wheel";
        wheel.body = b2CreateBody(world_, &wheelBody);

        b2ShapeDef shapeDef = vehicleShapeDef();
        shapeDef.density = wd.density;
        shapeDef.material.friction = tuning_.tireFriction;
        shapeDef.material.restitution = 0.0f;
        b2Circle circle{{0.0f, 0.0f}, wd.radius};
        b2CreateCircleShape(wheel.body, &shapeDef, &circle);

        b2WheelJointDef jointDef = b2DefaultWheelJointDef();
        jointDef.bodyIdA = chassis_;
        jointDef.bodyIdB = wheel.body;
        jointDef.localAnchorA = wd.anchor;
        jointDef.localAnchorB = {0.0f, 0.0f};
        jointDef.localAxisA = {0.0f, 1.0f};
        jointDef.enableSpring = true;
        jointDef.hertz = tuning_.suspHertz;
        jointDef.dampingRatio = tuning_.suspDamping;
        jointDef.enableLimit = true;
        jointDef.lowerTranslation = -tuning_.suspTravelDown;
        jointDef.upperTranslation = tuning_.suspTravelUp;
        jointDef.enableMotor = true;
        jointDef.maxMotorTorque = kCoastTorque;
        jointDef.motorSpeed = 0.0f;
        wheel.joint = b2CreateWheelJoint(world_, &jointDef);

        wheels_.push_back(wheel);
    }

    // Driver head on a sprung neck ---------------------------------------------
    b2BodyDef headDef = b2DefaultBodyDef();
    headDef.type = b2_dynamicBody;
    headDef.position = b2Add(position, b2Add(def.neckAnchor, {0.0f, def.headLift}));
    headDef.enableSleep = false;
    headDef.name = "head";
    head_ = b2CreateBody(world_, &headDef);

    b2ShapeDef headShape = vehicleShapeDef();
    headShape.density = kHeadMass / (kPi * def.headRadius * def.headRadius);
    headShape.material.friction = 0.5f;
    b2Circle headCircle{{0.0f, 0.0f}, def.headRadius};
    b2CreateCircleShape(head_, &headShape, &headCircle);

    b2RevoluteJointDef neckDef = b2DefaultRevoluteJointDef();
    neckDef.bodyIdA = chassis_;
    neckDef.bodyIdB = head_;
    neckDef.localAnchorA = def.neckAnchor;
    neckDef.localAnchorB = {0.0f, -def.headLift};
    neckDef.enableSpring = true;
    neckDef.hertz = kNeckHertz;
    neckDef.dampingRatio = kNeckDamping;
    neckDef.enableLimit = true;
    neckDef.lowerAngle = -kNeckLimit;
    neckDef.upperAngle = kNeckLimit;
    neck_ = b2CreateRevoluteJoint(world_, &neckDef);

    updateMassProperties();

    float shareSum = 0.0f;
    for (const Wheel& w : wheels_) shareSum += w.driveShare;
    for (Wheel& w : wheels_) w.driveShare = shareSum > 0.0f ? w.driveShare / shareSum : 0.0f;
}

void Vehicle::updateMassProperties()
{
    // Mass and rotational inertia of the whole vehicle about the chassis centre of mass.
    const b2Vec2 com = b2Body_GetWorldCenterOfMass(chassis_);
    totalMass_ = b2Body_GetMass(chassis_);
    systemInertia_ = b2Body_GetRotationalInertia(chassis_);
    auto addBody = [&](b2BodyId body) {
        const float m = b2Body_GetMass(body);
        totalMass_ += m;
        systemInertia_ += b2Body_GetRotationalInertia(body) + m * b2DistanceSquared(b2Body_GetWorldCenterOfMass(body), com);
    };
    for (const Wheel& w : wheels_) addBody(w.body);
    addBody(head_);
}

Vehicle::~Vehicle()
{
    // Destroying a body also destroys its joints and shapes.
    if (b2Body_IsValid(head_)) b2DestroyBody(head_);
    for (Wheel& w : wheels_) {
        if (b2Body_IsValid(w.body)) b2DestroyBody(w.body);
    }
    if (b2Body_IsValid(chassis_)) b2DestroyBody(chassis_);
}

void Vehicle::applyControls(float gas, float brake, float special, bool engineOn, bool nitroReady, float dt)
{
    (void)dt;
    preStepSpeed_ = speed();
    gas = clampf(gas, 0.0f, 1.0f);
    brake = clampf(brake, 0.0f, 1.0f);
    special = clampf(special, 0.0f, 1.0f);
    if (!engineOn) {
        gas = 0.0f;
        brake = 0.0f;  // an empty tank also kills the brakes, like the original
        special = 0.0f;
    }

    thrusting_ = false;
    boosting_ = false;

    for (Wheel& w : wheels_) {
        float wSpeed = 0.0f;
        float torque = kCoastTorque;
        if (gas > 0.0f && w.driveShare > 0.0f) {
            // Negative angular velocity = clockwise = rolling to the right.
            wSpeed = -tuning_.maxWheelSpeed;
            torque = tuning_.motorTorque * w.driveShare * gas;
        } else if (brake > 0.0f) {
            // The brake pedal brakes every wheel, then reverses once stopped.
            wSpeed = tuning_.reverseWheelSpeed;
            torque = tuning_.brakeTorque * brake;
        }
        b2WheelJoint_SetMotorSpeed(w.joint, wSpeed);
        b2WheelJoint_SetMaxMotorTorque(w.joint, torque);
    }

    // Pedals rotate the vehicle: gas tips it backwards (nose up), brake forwards.
    // The torque drives the spin rate towards control * airMaxSpin.
    const float control = engineOn ? gas - brake : 0.0f;
    lastAirTorque_ = 0.0f;
    if (control != 0.0f) {
        const float maxTorque = tuning_.airAccel * systemInertia_;
        const float spin = b2Body_GetAngularVelocity(chassis_);
        float torque = 0.0f;
        if (airborne()) {
            const float damping = maxTorque / std::fmax(0.5f, tuning_.airMaxSpin);
            torque = control * maxTorque - damping * spin;
            // Never push against the pedal direction harder than the pedal itself.
            torque = clampf(torque, -maxTorque, maxTorque);
        } else {
            torque = tuning_.groundTilt * control * maxTorque;
        }
        lastAirTorque_ = torque;
        b2Body_ApplyTorque(chassis_, torque, true);
    }

    if (engineOn && special > 0.0f && tuning_.thrusterAccel > 0.0f) {
        thrusting_ = true;
        const b2Vec2 up = b2Body_GetWorldVector(chassis_, {0.0f, 1.0f});
        b2Body_ApplyForceToCenter(chassis_, b2MulSV(totalMass_ * tuning_.thrusterAccel * special, up), true);
    } else if (engineOn && special > 0.0f && nitroReady && tuning_.nitroAccel > 0.0f) {
        boosting_ = true;
        const b2Vec2 fwd = b2Body_GetWorldVector(chassis_, {1.0f, 0.0f});
        b2Body_ApplyForceToCenter(chassis_, b2MulSV(totalMass_ * tuning_.nitroAccel * special, fwd), true);
    }

    if (tuning_.downforce > 0.0f && wheelsOnGround() > 0) {
        const b2Vec2 v = b2Body_GetLinearVelocity(chassis_);
        const float magnitude = tuning_.downforce * b2LengthSquared(v);
        const b2Vec2 down = b2Body_GetWorldVector(chassis_, {0.0f, -1.0f});
        b2Body_ApplyForceToCenter(chassis_, b2MulSV(magnitude, down), true);
    }
}

bool Vehicle::bodyTouching(b2BodyId body)
{
    b2ContactData contacts[8];
    const int count = b2Body_GetContactData(body, contacts, 8);
    for (int i = 0; i < count; ++i) {
        if (contacts[i].manifold.pointCount > 0) return true;
    }
    return false;
}

bool Vehicle::shapeTouching(b2ShapeId shape)
{
    if (!b2Shape_IsValid(shape)) return false;
    b2ContactData contacts[8];
    const int count = b2Shape_GetContactData(shape, contacts, 8);
    for (int i = 0; i < count; ++i) {
        if (contacts[i].manifold.pointCount > 0) return true;
    }
    return false;
}

void Vehicle::updateContacts(float dt)
{
    for (Wheel& w : wheels_) w.onGround = bodyTouching(w.body);
    chassisTouching_ = bodyTouching(chassis_);
    headTouching_ = bodyTouching(head_);

    rollCageCooldown_ = std::fmax(0.0f, rollCageCooldown_ - dt);
    rollCageEvent_ = RollCageEvent::None;
    if (rollCageHitsLeft_ > 0) {
        bool cageTouching = false;
        for (b2ShapeId s : rollCageShapes_) {
            if (shapeTouching(s)) {
                cageTouching = true;
                break;
            }
        }
        const float impactSpeed = std::fmax(speed(), preStepSpeed_);
        if ((cageTouching || headTouching_) && rollCageCooldown_ <= 0.0f &&
            (impactSpeed > 2.2f || headTouching_)) {
            rollCageHitsLeft_--;
            rollCageCooldown_ = 0.65f;
            headTouching_ = false;
            if (rollCageHitsLeft_ <= 0) {
                for (b2ShapeId s : rollCageShapes_) {
                    if (b2Shape_IsValid(s)) b2DestroyShape(s, true);
                }
                rollCageShapes_.clear();
                updateMassProperties();
                rollCageEvent_ = RollCageEvent::Broken;
            } else {
                rollCageEvent_ = RollCageEvent::Hit;
            }
        } else if (headTouching_ && rollCageCooldown_ > 0.0f) {
            headTouching_ = false;
        }
    }
}

RollCageEvent Vehicle::consumeRollCageEvent()
{
    const RollCageEvent e = rollCageEvent_;
    rollCageEvent_ = RollCageEvent::None;
    return e;
}

void Vehicle::rigidMove(float angle, b2Vec2 offset)
{
    const b2Vec2 pivot = b2Body_GetPosition(chassis_);
    const b2Rot rot = b2MakeRot(angle);
    auto move = [&](b2BodyId body) {
        const b2Transform xf = b2Body_GetTransform(body);
        const b2Vec2 p = b2Add(b2Add(pivot, b2RotateVector(rot, b2Sub(xf.p, pivot))), offset);
        b2Body_SetTransform(body, p, b2MulRot(rot, xf.q));
        b2Body_SetLinearVelocity(body, b2Vec2_zero);
        b2Body_SetAngularVelocity(body, 0.0f);
    };
    move(chassis_);
    move(head_);
    for (Wheel& w : wheels_) move(w.body);
}

int Vehicle::wheelsOnGround() const
{
    int n = 0;
    for (const Wheel& w : wheels_) n += w.onGround ? 1 : 0;
    return n;
}

bool Vehicle::rearWheelOnGround() const
{
    if (wheels_.empty()) return false;
    const size_t half = std::max<size_t>(1, wheels_.size() / 2);
    for (size_t i = 0; i < half; ++i) {
        if (wheels_[i].onGround) return true;
    }
    return false;
}

bool Vehicle::frontWheelOnGround() const
{
    return !wheels_.empty() && wheels_.back().onGround;
}

b2Vec2 Vehicle::position() const { return b2Body_GetPosition(chassis_); }

b2Vec2 Vehicle::velocity() const { return b2Body_GetLinearVelocity(chassis_); }

float Vehicle::angle() const { return b2Rot_GetAngle(b2Body_GetRotation(chassis_)); }

float Vehicle::angularVelocity() const { return b2Body_GetAngularVelocity(chassis_); }

float Vehicle::speed() const { return b2Length(velocity()); }

float Vehicle::wheelSpinRatio() const
{
    float best = 0.0f;
    for (const Wheel& w : wheels_) {
        if (w.driveShare <= 0.0f) continue;
        best = std::fmax(best, std::fabs(b2Body_GetAngularVelocity(w.body)));
    }
    return tuning_.maxWheelSpeed > 0.0f ? best / tuning_.maxWheelSpeed : 0.0f;
}

b2Transform Vehicle::chassisTransform() const { return b2Body_GetTransform(chassis_); }

b2Transform Vehicle::wheelTransform(int i) const
{
    return b2Body_GetTransform(wheels_[static_cast<size_t>(i)].body);
}

b2Transform Vehicle::headTransform() const { return b2Body_GetTransform(head_); }

float Vehicle::wheelAngularVelocity(int i) const
{
    return b2Body_GetAngularVelocity(wheels_[static_cast<size_t>(i)].body);
}

b2Vec2 Vehicle::wheelMountWorld(int i) const
{
    return b2Body_GetWorldPoint(chassis_, wheels_[static_cast<size_t>(i)].mount);
}

void Vehicle::probes(std::vector<Probe>& out) const
{
    out.clear();
    out.push_back({b2Body_GetPosition(chassis_), probeRadius_});
    for (const Wheel& w : wheels_) out.push_back({b2Body_GetPosition(w.body), w.radius});
    out.push_back({b2Body_GetPosition(head_), def_->headRadius});
}

} // namespace hr
