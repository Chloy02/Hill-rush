#pragma once

#include "game/Hud.hpp"
#include "game/Particles.hpp"
#include "game/WorldView.hpp"
#include "sim/Pose.hpp"
#include "sim/Session.hpp"

#include <memory>
#include <vector>

namespace hr {

class App;

// A run being played on screen: the simulation plus its camera, particles,
// HUD, sounds and effects. Single-player runs and online races both use one.
class LiveRun {
public:
    // `stage` must outlive the run (a stages() entry); `difficulty` reshapes it.
    LiveRun(App& app, const StageDef& stage, Difficulty difficulty, const VehicleDef& vehicle,
            const UpgradeLevels& upgrades, float bestDistance, int levelsCompleted);

    // One physics step with these pedals, plus the sounds and effects of
    // whatever happened in it.
    void step(const InputState& input);
    // Animation for one rendered frame. The camera follows our car, or
    // `focus` when given (watching someone else).
    void update(float dt, const b2Vec2* focus = nullptr, b2Vec2 focusVelocity = {0.0f, 0.0f});
    // `frozen` shows the last physics state as is (paused, results).
    void drawWorld(float time, bool frozen, bool debug, const std::vector<OtherCar>& others = {}) const;

    Session& session() { return *session_; }
    const Session& session() const { return *session_; }
    Hud& hud() { return hud_; }
    const Hud& hud() const { return hud_; }
    WorldView& view() { return view_; }
    const WorldView& view() const { return view_; }
    const Particles& particles() const { return particles_; }
    // Our car, interpolated between the last two physics steps.
    VehiclePose pose() const;
    const StageDef& stage() const { return stage_; }
    const VehicleDef& vehicle() const { return vehicle_; }
    Difficulty difficulty() const { return difficulty_; }
    int steps() const { return steps_; }

private:
    void handleEvent(const GameEvent& e);
    void emitEffects();

    App& app_;
    const StageDef& stage_;
    const VehicleDef& vehicle_;
    Difficulty difficulty_;
    std::unique_ptr<Session> session_;
    WorldView view_;
    Particles particles_;
    Hud hud_;
    VehiclePose prevPose_;
    VehiclePose currPose_;
    int steps_ = 0;
};

} // namespace hr
