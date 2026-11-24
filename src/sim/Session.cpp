#include "sim/Session.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"

#include <algorithm>
#include <cmath>

namespace hr {

namespace {

constexpr float kViewBehind = 70.0f;
constexpr float kViewAhead = 110.0f;
constexpr int kAirborneFrames = 5;    // frames without contact before we call it a jump
constexpr int kLandedFrames = 3;      // frames of contact before we call it a landing
constexpr float kNeckFlipWindow = 1.0f;  // a crash this soon after a flip is a "neck flip"
constexpr float kUpsideDownLimit = 3.0f;

// Friction mixing: tyre grip multiplied by surface grip reads naturally
// ("ice has a tenth of the grip").
float multiplyFriction(float frictionA, int, float frictionB, int)
{
    return frictionA * frictionB;
}

float vehicleRestHeight(const VehicleDef& def)
{
    float lowest = 0.0f;
    for (const WheelDef& w : def.wheels) lowest = std::fmin(lowest, w.anchor.y - w.radius);
    return -lowest + 0.05f;
}

} // namespace

int airTimeStep(float seconds)
{
    if (seconds < 2.0f) return 0;
    return static_cast<int>(std::floor((seconds - 2.0f) / 0.25f)) + 1;
}

int airTimeBonus(float seconds)
{
    const int step = airTimeStep(seconds);
    if (step <= 0) return 0;
    if (step <= 4) return 25 * step;
    if (step <= 8) return 100 * (step - 3);
    return std::min(25000, 1000 + 500 * (step - 9));
}

const char* airTimeLabel(int step)
{
    if (step <= 4) return "AIR TIME";
    if (step <= 8) return "BIG AIR TIME";
    return "INSANE AIR TIME";
}

int wheelieStep(float seconds)
{
    if (seconds < 2.0f) return 0;
    return static_cast<int>(std::floor((seconds - 2.0f) / 0.25f)) + 1;
}

int wheelieBonus(float seconds)
{
    const int step = wheelieStep(seconds);
    if (step <= 0) return 0;
    return std::min(5000, 100 + 50 * (step - 1));
}

const char* wheelieLabel(int step)
{
    if (step <= 4) return "WHEELIE";
    if (step <= 8) return "SUPER WHEELIE";
    return "INSANE WHEELIE";
}

Session::Session(const StageDef& stage, const VehicleDef& vehicle, const UpgradeLevels& upgrades,
                 float bestDistance, int levelsCompleted)
    : stage_(stage), vehicleDef_(&vehicle), bestDistance_(bestDistance), levelsCompleted_(levelsCompleted)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -stage_.gravity};
    worldDef.frictionCallback = multiplyFriction;
    worldDef.enableSleep = false;
    world_ = b2CreateWorld(&worldDef);

    terrain_ = std::make_unique<Terrain>(world_, stage_);
    terrain_->ensureRange(startX() - kViewBehind, startX() + kViewAhead);

    VehicleTuning tuning = computeTuning(vehicle, upgrades);
    tuning.fuelSeconds *= stage_.fuelTankScale;
    const b2Vec2 spawn{startX(), terrain_->surfaceHeight(startX()) + vehicleRestHeight(vehicle)};
    vehicle_ = std::make_unique<Vehicle>(world_, vehicle, tuning, spawn);

    pickups_ = std::make_unique<Pickups>(stage_, *terrain_);
    pickups_->ensureRange(startX() - kViewBehind, startX() + kViewAhead);

    maxX_ = startX();
    lastAngle_ = vehicle_->angle();
    nitro_ = vehicle.hasThrusters() ? 0.0f : 0.35f;
    LOG_INFO("Session start: stage=%s (%s) vehicle=%s torque=%.0f speed=%.1f grip=%.2f fuel=%.0fs 4wd=%.2f",
             stage_.id.c_str(), difficultyDef(stage_.difficulty).key, vehicle.id.c_str(), tuning.motorTorque,
             tuning.maxWheelSpeed, tuning.tireFriction, tuning.fuelSeconds, tuning.frontDriveShare);
}

Session::~Session()
{
    // Our wrappers destroy their bodies, so they must go before the world.
    pickups_.reset();
    vehicle_.reset();
    terrain_.reset();
    b2DestroyWorld(world_);
}

void Session::step(const InputState& input)
{
    if (over_) return;
    const float dt = kFixedDt;
    time_ += dt;

    const bool engineOn = state_ == RunState::Running;
    const float gas = engineOn ? input.gas : 0.0f;
    const float brake = engineOn ? input.brake : 0.0f;
    const float special = engineOn ? input.special : 0.0f;
    const bool nitroReady = engineOn && hasNitro() && nitro_ > 0.0f;
    vehicle_->applyControls(gas, brake, special, engineOn, nitroReady, dt);
    throttle_ = gas;

    // Fuel drains with time (and faster while firing thrusters).
    if (state_ == RunState::Running) {
        const float fuelRate = vehicle_->thrusting() ? (1.0f + vehicle_->tuning().thrusterFuelRate) : 1.0f;
        fuel_ -= dt * fuelRate / vehicle_->tuning().fuelSeconds;
        if (fuel_ <= kLowFuel && !lowFuelWarned_) {
            lowFuelWarned_ = true;
            emit({EventType::LowFuel, vehicle_->position()});
        }
        if (fuel_ <= 0.0f) {
            fuel_ = 0.0f;
            state_ = RunState::OutOfFuel;
            endTimer_ = 0.0f;
            emit({EventType::OutOfFuel, vehicle_->position()});
        }
        if (vehicle_->boosting()) {
            nitro_ = clampf(nitro_ - 0.55f * dt, 0.0f, 1.0f);
        }
    }

    b2World_Step(world_, dt, kSubSteps);
    vehicle_->updateContacts(dt);

    if (state_ != RunState::Crashed) {
        const RollCageEvent cageEvt = vehicle_->consumeRollCageEvent();
        if (cageEvt == RollCageEvent::Hit) {
            GameEvent e{EventType::RollCageHit, b2Body_GetPosition(vehicle_->headBody())};
            e.count = vehicle_->rollCageHitsLeft();
            emit(e);
        } else if (cageEvt == RollCageEvent::Broken) {
            emit({EventType::RollCageBreak, b2Body_GetPosition(vehicle_->headBody())});
        }
    } else {
        (void)vehicle_->consumeRollCageEvent();
    }

    const b2Vec2 pos = vehicle_->position();
    if (!std::isfinite(pos.x) || !std::isfinite(pos.y)) {
        // Should never happen; end the run rather than propagate NaNs.
        LOG_ERROR("Vehicle position became non-finite; ending run");
        finish(EndReason::DriverDown);
        return;
    }

    terrain_->ensureRange(pos.x - kViewBehind, pos.x + kViewAhead);
    pickups_->ensureRange(pos.x - kViewBehind, pos.x + kViewAhead);

    if (pos.x > maxX_) maxX_ = pos.x;
    result_.distance = std::fmax(0.0f, maxX_ - startX());
    result_.duration = time_;
    if (!recordAnnounced_ && bestDistance_ > 50.0f && result_.distance > bestDistance_) {
        recordAnnounced_ = true;
        result_.newRecord = true;
        emit({EventType::NewRecord, pos});
    }
    if (state_ != RunState::Crashed) checkLevels();

    // Engine note: follows the driven wheels, revs up when the throttle is open.
    const float spin = clampf(vehicle_->wheelSpinRatio(), 0.0f, 1.15f);
    float targetRpm = 0.12f + 0.85f * spin;
    if (gas > 0.0f) targetRpm = std::fmax(targetRpm, 0.3f + 0.25f * gas);
    if (!engineOn) targetRpm = 0.0f;
    rpm_ = approach(rpm_, targetRpm, 7.0f, dt);

    // Pickups (a crashed driver collects nothing).
    if (state_ != RunState::Crashed) {
        vehicle_->probes(probes_);
        collected_.clear();
        pickups_->collect(probes_, collected_);
        for (const Pickup& p : collected_) {
            if (p.kind == PickupKind::Coin) {
                result_.pickupCoins += p.value;
                addNitro(0.02f);
                emit({EventType::Coin, p.pos, p.value});
            } else {
                const bool wasOut = state_ == RunState::OutOfFuel;
                fuel_ = 1.0f;
                lowFuelWarned_ = false;
                if (wasOut) {
                    state_ = RunState::Running;
                    result_.bonusCoins += kLuckyFuelBonus;
                    result_.luckyFuels++;
                    emit({EventType::LuckyFuel, p.pos, kLuckyFuelBonus});
                }
                emit({EventType::Fuel, p.pos});
            }
        }
    }

    if (state_ != RunState::Crashed && vehicle_->headTouchingGround()) crash();

    // A vehicle resting on its back can't recover; call it after a few seconds.
    const bool upsideDown = std::fabs(wrapAngle(vehicle_->angle())) > 2.0f;
    upsideDownTimer_ = upsideDown && vehicle_->speed() < 0.6f ? upsideDownTimer_ + dt : 0.0f;
    if (state_ != RunState::Crashed && upsideDownTimer_ > kUpsideDownLimit) crash();

    updateBonuses();
    updateWheelie(dt);

    // End conditions.
    if (state_ == RunState::Crashed) {
        endTimer_ += dt;
        if (endTimer_ >= kCrashLinger) finish(EndReason::DriverDown);
    } else if (state_ == RunState::OutOfFuel) {
        endTimer_ += dt;
        stillTimer_ = vehicle_->speed() < 0.3f ? stillTimer_ + dt : 0.0f;
        if (stillTimer_ > 1.0f || endTimer_ > kCoastTimeout) finish(EndReason::OutOfFuel);
    } else {
        endTimer_ = 0.0f;
        stillTimer_ = 0.0f;
    }
}

void Session::crash()
{
    state_ = RunState::Crashed;
    endTimer_ = 0.0f;
    emit({EventType::Crash, b2Body_GetPosition(vehicle_->headBody())});

    // Snapping your neck mid-flip pays a consolation bonus once per run.
    const bool flipping = airborne_ && jumpBackFlips_ + jumpFrontFlips_ > 0;
    if (result_.neckFlips == 0 && (flipping || time_ - lastFlipTime_ <= kNeckFlipWindow)) {
        result_.neckFlips = 1;
        result_.bonusCoins += kNeckFlipBonus;
        emit({EventType::NeckFlip, vehicle_->position(), kNeckFlipBonus});
    }
}

void Session::checkLevels()
{
    while (result_.distance >= levelDistance(stage_, levelsCompleted_ + 1)) {
        levelsCompleted_++;
        const long long reward =
            static_cast<long long>(std::llround(static_cast<double>(levelReward(levelsCompleted_)) * stage_.levelRewardScale));
        result_.levelCoins += reward;
        result_.levelsReached++;
        GameEvent e{EventType::LevelUp, vehicle_->position()};
        e.value = static_cast<int>(reward);
        e.count = levelsCompleted_;
        emit(e);
    }
}

void Session::addNitro(float amount)
{
    if (hasNitro()) nitro_ = clampf(nitro_ + amount, 0.0f, 1.0f);
}

void Session::updateBonuses()
{
    const bool contact = !vehicle_->airborne();
    if (contact) {
        groundFrames_++;
        airFrames_ = 0;
    } else {
        airFrames_++;
        groundFrames_ = 0;
    }

    const float angle = vehicle_->angle();
    if (!airborne_ && airFrames_ >= kAirborneFrames) {
        airborne_ = true;
        // The jump really started when contact was lost.
        airStart_ = time_ - static_cast<float>(airFrames_) * kFixedDt;
        rotation_ = 0.0f;
        jumpBackFlips_ = 0;
        jumpFrontFlips_ = 0;
        peakFallSpeed_ = 0.0f;
        lastAngle_ = angle;
    }
    if (!airborne_) return;

    rotation_ += wrapAngle(angle - lastAngle_);
    lastAngle_ = angle;
    peakFallSpeed_ = std::fmax(peakFallSpeed_, -vehicle_->velocity().y);

    if (state_ != RunState::Crashed) {
        // Counter-clockwise (positive) rotation while heading right is a back flip.
        auto award = [&](bool back) {
            const int number = jumpBackFlips_ + jumpFrontFlips_;
            result_.bonusCoins += kFlipBonus;
            (back ? result_.backFlips : result_.frontFlips)++;
            lastFlipTime_ = time_;
            addNitro(0.40f);
            emit({EventType::Flip, vehicle_->position(), kFlipBonus, 0.0f, 0.0f, number, back});
        };
        while (rotation_ >= kFlipAngle + kTwoPi * static_cast<float>(jumpBackFlips_)) {
            jumpBackFlips_++;
            award(true);
        }
        while (-rotation_ >= kFlipAngle + kTwoPi * static_cast<float>(jumpFrontFlips_)) {
            jumpFrontFlips_++;
            award(false);
        }
    }

    if (groundFrames_ >= kLandedFrames) {
        airborne_ = false;
        const float airTime = time_ - airStart_ - static_cast<float>(groundFrames_) * kFixedDt;
        result_.bestAirTime = std::fmax(result_.bestAirTime, airTime);
        emit({EventType::Landing, vehicle_->position(), 0, airTime, peakFallSpeed_});
        const int bonus = airTimeBonus(airTime);
        if (state_ != RunState::Crashed && bonus > 0) {
            const int step = airTimeStep(airTime);
            result_.bonusCoins += bonus;
            result_.airTimes++;
            addNitro(0.20f + 0.04f * static_cast<float>(step));
            GameEvent e{EventType::AirTime, vehicle_->position(), bonus, airTime};
            e.count = step;
            emit(e);
        }
        const int jumpFlips = jumpBackFlips_ + jumpFrontFlips_;
        if (state_ != RunState::Crashed && jumpFlips > 0 && !vehicle_->chassisTouchingGround() &&
            std::fabs(wrapAngle(vehicle_->angle() - terrain_->surfaceAngle(vehicle_->position().x))) < 0.35f) {
            const int cleanBonus = kCleanLandingBonus * jumpFlips;
            result_.bonusCoins += cleanBonus;
            result_.cleanLandings++;
            addNitro(0.25f);
            GameEvent e{EventType::CleanLanding, vehicle_->position(), cleanBonus};
            e.count = jumpFlips;
            emit(e);
        }
    }
}

void Session::updateWheelie(float dt)
{
    const bool active = state_ == RunState::Running && !airborne_ && vehicle_->wheelCount() >= 2 &&
                        vehicle_->rearWheelOnGround() && !vehicle_->frontWheelOnGround() &&
                        vehicle_->velocity().x > 1.5f;
    if (active) {
        wheelieGrace_ = 0;
        wheelieTime_ += dt;
        return;
    }
    if (wheelieTime_ <= 0.0f) return;
    if (state_ == RunState::Crashed) {
        wheelieTime_ = 0.0f;
        wheelieGrace_ = 0;
        return;
    }
    if (++wheelieGrace_ <= 6) return;

    if (wheelieTime_ >= 2.0f) {
        const int bonus = wheelieBonus(wheelieTime_);
        const int step = wheelieStep(wheelieTime_);
        if (bonus > 0) {
            result_.bonusCoins += bonus;
            result_.wheelies++;
            addNitro(0.25f + 0.04f * static_cast<float>(step));
            GameEvent e{EventType::Wheelie, vehicle_->position(), bonus, wheelieTime_};
            e.count = step;
            emit(e);
        }
    }
    wheelieTime_ = 0.0f;
    wheelieGrace_ = 0;
}

void Session::finish(EndReason reason)
{
    if (over_) return;
    over_ = true;
    result_.reason = reason;
    result_.distance = std::fmax(0.0f, maxX_ - startX());
    result_.duration = time_;
    if (result_.distance > bestDistance_) result_.newRecord = true;
    LOG_INFO("Run over: %s distance=%.1f coins=%lld (+%lld bonus, +%lld levels) flips=%d time=%.1fs",
             endReasonText(reason), result_.distance, result_.pickupCoins, result_.bonusCoins, result_.levelCoins,
             result_.flips(), time_);
}

const char* endReasonText(EndReason reason)
{
    switch (reason) {
    case EndReason::None: return "Running";
    case EndReason::DriverDown: return "Driver Down";
    case EndReason::OutOfFuel: return "Out of Fuel";
    }
    return "?";
}

} // namespace hr
