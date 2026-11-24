#pragma once

#include "sim/Defs.hpp"
#include "sim/Pickups.hpp"
#include "sim/Terrain.hpp"
#include "sim/Vehicle.hpp"

#include <box2d/box2d.h>

#include <memory>
#include <string>
#include <vector>

namespace hr {

inline constexpr float kFixedDt = 1.0f / 60.0f;
inline constexpr int kSubSteps = 8;

struct InputState {
    float gas = 0.0f;    // 0..1
    float brake = 0.0f;  // 0..1
};

enum class RunState {
    Running,
    OutOfFuel,  // coasting; a fuel can still rescues the run
    Crashed,    // driver's head hit the ground
};

enum class EndReason { None, DriverDown, OutOfFuel };

enum class EventType {
    Coin,       // value = coin value
    Fuel,
    AirTime,    // value = coins, seconds = air time, count = bonus step (for the label)
    Flip,       // value = coins, count = flips so far this jump, back = backflip
    NeckFlip,   // value = coins
    LevelUp,    // value = coins, count = level number
    LowFuel,
    OutOfFuel,
    Crash,
    NewRecord,
    Landing,    // seconds = air time, impact = fall speed (m/s)
};

struct GameEvent {
    EventType type;
    b2Vec2 pos{};
    int value = 0;
    float seconds = 0.0f;
    float impact = 0.0f;
    int count = 0;
    bool back = false;
};

struct RunResult {
    EndReason reason = EndReason::None;
    float distance = 0.0f;
    long long pickupCoins = 0;
    long long bonusCoins = 0;  // air time, flips, neck flip
    long long levelCoins = 0;  // one-time level checkpoint rewards
    int airTimes = 0;
    int backFlips = 0;
    int frontFlips = 0;
    int neckFlips = 0;
    int levelsReached = 0;     // levels newly completed this run
    float bestAirTime = 0.0f;
    float duration = 0.0f;
    bool newRecord = false;

    int flips() const { return backFlips + frontFlips; }
    long long totalCoins() const { return pickupCoins + bonusCoins + levelCoins; }
};

// Air-time bonus for a jump of `seconds`, as in the original: nothing until
// 2 s, then a step every 0.25 s (25, 50, 75, 100, 200 ... 500, 1000, 1500 ...).
int airTimeStep(float seconds);
int airTimeBonus(float seconds);
const char* airTimeLabel(int step);

// One run of one vehicle on one stage.
class Session {
public:
    static constexpr float kLowFuel = 0.20f;
    static constexpr float kCrashLinger = 2.4f;       // seconds of wreck before the results
    static constexpr float kCoastTimeout = 6.0f;      // out of fuel: max seconds of coasting
    static constexpr float kFlipAngle = 3.0f * 3.14159265f / 2.0f;  // 270 degrees counts as a flip
    static constexpr int kFlipBonus = 1000;
    static constexpr int kNeckFlipBonus = 2500;

    // `levelsCompleted` is how many level checkpoints of this stage the player
    // has already been paid for; only newer levels pay out again.
    Session(const StageDef& stage, const VehicleDef& vehicle, const UpgradeLevels& upgrades,
            float bestDistance, int levelsCompleted = 0);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Advances the simulation by one fixed step (kFixedDt).
    void step(const InputState& input);

    bool isOver() const { return over_; }
    RunState state() const { return state_; }
    const RunResult& result() const { return result_; }

    // Events produced since the last call; the caller drains them each frame.
    std::vector<GameEvent>& events() { return events_; }

    float distance() const { return result_.distance; }
    float bestDistance() const { return bestDistance_; }
    float fuel() const { return fuel_; }
    float time() const { return time_; }
    long long coins() const { return result_.totalCoins(); }
    bool airborne() const { return airborne_; }
    float airTime() const { return airborne_ ? time_ - airStart_ : 0.0f; }
    float engineRpm() const { return rpm_; }  // 0..1, smoothed
    float throttle() const { return throttle_; }
    int levelsCompleted() const { return levelsCompleted_; }
    float nextLevelDistance() const { return levelDistance(stage_, levelsCompleted_ + 1); }

    const StageDef& stage() const { return stage_; }
    const Vehicle& vehicle() const { return *vehicle_; }
    Vehicle& vehicleForTesting() { return *vehicle_; }
    const Terrain& terrain() const { return *terrain_; }
    const Pickups& pickups() const { return *pickups_; }

    static float startX() { return 0.0f; }

private:
    void updateBonuses();
    void checkLevels();
    void crash();
    void finish(EndReason reason);
    void emit(const GameEvent& e) { events_.push_back(e); }

    StageDef stage_;  // a copy: callers may pass a temporary (e.g. applyDifficulty())
    const VehicleDef* vehicleDef_;
    b2WorldId world_{};
    std::unique_ptr<Terrain> terrain_;
    std::unique_ptr<Vehicle> vehicle_;
    std::unique_ptr<Pickups> pickups_;

    std::vector<GameEvent> events_;
    std::vector<Vehicle::Probe> probes_;
    std::vector<Pickup> collected_;

    RunState state_ = RunState::Running;
    RunResult result_;
    bool over_ = false;
    float bestDistance_ = 0.0f;
    int levelsCompleted_ = 0;
    bool recordAnnounced_ = false;
    float time_ = 0.0f;
    float fuel_ = 1.0f;
    bool lowFuelWarned_ = false;
    float endTimer_ = 0.0f;
    float stillTimer_ = 0.0f;
    float upsideDownTimer_ = 0.0f;
    float maxX_ = 0.0f;
    float rpm_ = 0.1f;
    float throttle_ = 0.0f;

    // Air time / flip tracking
    bool airborne_ = false;
    int airFrames_ = 0;
    int groundFrames_ = 0;
    float airStart_ = 0.0f;
    float lastAngle_ = 0.0f;
    float rotation_ = 0.0f;
    int jumpBackFlips_ = 0;
    int jumpFrontFlips_ = 0;
    float lastFlipTime_ = -100.0f;
    float peakFallSpeed_ = 0.0f;
};

const char* endReasonText(EndReason reason);

} // namespace hr
