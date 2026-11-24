#include "game/LiveRun.hpp"

#include "core/Math.hpp"
#include "game/App.hpp"
#include "game/Palette.hpp"
#include "game/VehicleArt.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

LiveRun::LiveRun(App& app, const StageDef& stage, Difficulty difficulty, const VehicleDef& vehicle,
                 const UpgradeLevels& upgrades, float bestDistance, int levelsCompleted)
    : app_(app), stage_(stage), vehicle_(vehicle), difficulty_(difficulty)
{
    session_ = std::make_unique<Session>(applyDifficulty(stage, difficulty), vehicle, upgrades, bestDistance,
                                         levelsCompleted);
    view_.reset(stage, session_->vehicle().position());
    currPose_ = poseOf(*session_);
    prevPose_ = currPose_;
}

void LiveRun::step(const InputState& input)
{
    prevPose_ = currPose_;
    session_->step(input);
    currPose_ = poseOf(*session_);
    steps_++;
    for (const GameEvent& e : session_->events()) handleEvent(e);
    session_->events().clear();
    emitEffects();
}

void LiveRun::update(float dt, const b2Vec2* focus, b2Vec2 focusVelocity)
{
    particles_.update(dt);
    hud_.update(dt, *session_);
    if (focus) view_.update(dt, *focus, focusVelocity);
    else view_.update(dt, pose().chassis.p, session_->vehicle().velocity());
}

VehiclePose LiveRun::pose() const { return lerpPose(prevPose_, currPose_, app_.interpolation()); }

void LiveRun::drawWorld(float time, bool frozen, bool debug, const std::vector<OtherCar>& others) const
{
    view_.draw(*session_, frozen ? currPose_ : pose(), particles_, app_.assets(), time, debug, others);
}

void LiveRun::emitEffects()
{
    const Vehicle& v = session_->vehicle();
    const Palette& pal = paletteFor(stage_.id);
    const b2Transform chassis = v.chassisTransform();

    // Exhaust puffs, more often with the throttle open.
    const int every = session_->throttle() > 0.5f ? 3 : 7;
    if (session_->state() == RunState::Running && steps_ % every == 0) {
        const b2Vec2 at = b2TransformPoint(chassis, exhaustPoint(vehicle_));
        const b2Vec2 dir = b2RotateVector(chassis.q, exhaustDirection(vehicle_));
        particles_.exhaust(at, dir, session_->throttle());
    }

    // Dust from wheels that are slipping or moving fast on the ground.
    if (steps_ % 2 != 0) return;
    const b2Vec2 vel = v.velocity();
    for (int i = 0; i < v.wheelCount(); ++i) {
        if (!v.wheelOnGround(i)) continue;
        const float r = v.wheelRadius(i);
        const float rimSpeed = -v.wheelAngularVelocity(i) * r;
        const float slip = std::fabs(rimSpeed - b2Length(vel) * (vel.x >= 0.0f ? 1.0f : -1.0f));
        const float strength = clampf(slip / 6.0f, 0.0f, 1.0f) + (v.speed() > 8.0f ? 0.2f : 0.0f);
        if (strength < 0.25f) continue;
        const b2Vec2 hub = v.wheelTransform(i).p;
        const b2Vec2 contact{hub.x - r * 0.3f, hub.y - r * 0.9f};
        particles_.dust(contact, {rimSpeed > 0.0f ? -1.0f : 1.0f, 0.4f}, pal.dust, strength);
    }
}

void LiveRun::handleEvent(const GameEvent& e)
{
    hud_.onEvent(e, *session_);
    Audio& audio = app_.audio();
    char text[32];
    switch (e.type) {
    case EventType::Coin: {
        const float pitch = e.value >= 500 ? 1.35f : e.value >= 100 ? 1.22f : e.value >= 25 ? 1.1f : 1.0f;
        audio.play(Sfx::Coin, pitch, 0.8f);
        particles_.sparkle(e.pos, {255, 214, 60, 255}, 5);
        std::snprintf(text, sizeof(text), "+%d", e.value);
        particles_.popup(e.pos, text, {255, 230, 120, 255}, 0.5f);
        break;
    }
    case EventType::Fuel:
        audio.play(Sfx::Fuel);
        particles_.sparkle(e.pos, {140, 240, 120, 255}, 10);
        particles_.popup(e.pos, "FUEL", {140, 240, 120, 255}, 0.6f);
        break;
    case EventType::Flip:
        audio.play(Sfx::Flip);
        break;
    case EventType::AirTime:
        audio.play(Sfx::Flip, 0.85f, 0.8f);
        break;
    case EventType::NeckFlip:
        audio.play(Sfx::Flip, 0.7f);
        break;
    case EventType::LevelUp:
        audio.play(Sfx::Fanfare);
        break;
    case EventType::NewRecord:
        audio.play(Sfx::Fanfare, 1.12f, 0.8f);
        break;
    case EventType::LowFuel:
        audio.play(Sfx::LowFuel);
        break;
    case EventType::OutOfFuel:
        hud_.shout("OUT OF FUEL!", {255, 150, 60, 255}, 3.0f);
        audio.play(Sfx::LowFuel, 0.8f);
        break;
    case EventType::Crash:
        audio.play(Sfx::Crash);
        view_.shake(0.35f, 0.5f);
        particles_.dizzyStars(e.pos);
        particles_.burst(e.pos, paletteFor(stage_.id).dust, 18, 5.0f);
        hud_.shout("DRIVER DOWN!", {255, 80, 60, 255}, 3.0f);
        break;
    case EventType::Landing:
        if (e.impact > 5.0f) {
            audio.play(Sfx::Land, 1.0f, clampf(e.impact / 14.0f, 0.3f, 1.0f));
            particles_.burst({e.pos.x, session_->terrain().surfaceHeight(e.pos.x)}, paletteFor(stage_.id).dust,
                             static_cast<int>(clampf(e.impact, 4.0f, 16.0f)), e.impact * 0.4f);
        }
        if (e.impact > 11.0f) view_.shake(0.12f, 0.25f);
        break;
    }
}

} // namespace hr
