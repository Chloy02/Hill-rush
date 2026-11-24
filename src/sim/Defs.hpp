#pragma once

#include <box2d/math_functions.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace hr {

// ---------------------------------------------------------------------------
// Stages
// ---------------------------------------------------------------------------

// One layer of terrain noise. Amplitude grows from ampStart to ampEnd as the
// player gets further from the start line (StageDef::rampDistance).
struct TerrainOctave {
    float wavelength;
    float ampStart;
    float ampEnd;
};

enum class TerrainFeature {
    None,
    Craters,     // moon-style bowls with raised rims
    IcePatches,  // slippery stretches with much lower friction
    CaveCeiling, // low ceiling with hanging stalactites
    MudPits,     // deep sticky mud stretches and plank bridges
};

enum class Difficulty { Easy, Medium, Hard };

struct StageDef {
    std::string id;
    std::string name;
    std::string blurb;
    int price = 0;                 // coins to unlock (0 = unlocked from the start)
    float gravity = 9.8f;          // m/s^2, downwards
    float groundFriction = 0.9f;
    float rollingResistance = 0.0f;  // sand drags on the wheels
    uint64_t seed = 1;             // the track layout is fixed per stage, like the original
    float rampDistance = 2500.0f;  // metres until terrain reaches full difficulty
    std::array<TerrainOctave, 3> octaves{};
    // Sharpening of the height profile (tanh S-curve). 1 keeps the noise as is;
    // larger values flatten hilltops and valleys and steepen the faces between.
    float steepStart = 1.0f;
    float steepEnd = 1.0f;
    // Hard cap on slope (degrees), growing from start to end over rampDistance.
    // Keeps every hill climbable by a well-upgraded vehicle with momentum.
    float maxSlopeStart = 40.0f;
    float maxSlopeEnd = 52.0f;
    TerrainFeature feature = TerrainFeature::None;
    float iceFriction = 0.12f;     // used by TerrainFeature::IcePatches
    float mudFriction = 0.65f;     // used by TerrainFeature::MudPits
    float mudRollingResistance = 0.24f;
    float ceilingBase = 8.6f;      // nominal clearance above ground on Cave
    float ceilingMin = 4.9f;       // hard minimum clearance so tall vehicles never pinch

    // Fuel can n (0-based) sits at fuelFirst + n*fuelGap + fuelGapGrowth*n(n-1)/2.
    float fuelFirst = 90.0f;
    float fuelGap = 90.0f;
    float fuelGapGrowth = 2.0f;

    // A coin cluster every ~30 m is worth coinRate * distance (capped), like the original.
    float coinRate = 1.2f;

    // One-time level checkpoints: level L is at levelSpacing*L + levelOffset metres.
    float levelSpacing = 200.0f;
    float levelOffset = 0.0f;

    // Set by applyDifficulty(); Easy leaves every value above untouched.
    Difficulty difficulty = Difficulty::Easy;
    float extraBumps = 0.0f;        // extra rough ground: bumps of 0.5 m rising to 1.4 m, times this
    float fuelTankScale = 1.0f;     // multiplies every vehicle's tank duration
    float levelRewardScale = 1.0f;  // multiplies level checkpoint rewards
};

// ---------------------------------------------------------------------------
// Difficulty
// ---------------------------------------------------------------------------

// How a difficulty reshapes a stage. Vehicle handling, the big hills and the
// slope limits are the same in every difficulty (so harder modes add no
// impassable walls); harder modes add rough ground, cut the fuel supply and
// pay more for every metre.
struct DifficultyDef {
    Difficulty id;
    const char* key;         // stable id used in save files ("easy")
    const char* name;        // "Easy"
    const char* blurb;
    float extraBumps;        // extra rough ground (see StageDef::extraBumps)
    float fuelTankScale;     // full-tank duration
    float fuelGrowthScale;   // how fast the gaps between fuel cans widen
    float rewardScale;       // coins and level rewards
};

const std::array<DifficultyDef, 3>& difficulties();
const DifficultyDef& difficultyDef(Difficulty d);
// Unknown keys map to Easy.
Difficulty difficultyFromKey(const std::string& key);
// The stage as played on the given difficulty. Easy returns it unchanged.
StageDef applyDifficulty(const StageDef& base, Difficulty d);

// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

enum class UpgradeKind {
    Engine,       // more torque and top speed
    Suspension,   // stiffer, better damped, more travel
    Tires,        // grip
    FourWheelDrive,
    FuelTank,     // fuel lasts longer
    AirControl,   // rotation torque while airborne
    Downforce,    // presses the car onto the road at speed
    Thrusters,    // upward rocket lift
    RollCage,     // extra roof impacts absorbed before breaking
};

// Levels run from 1 (stock) to maxLevel. Buying level L costs costFactor * L^2.
struct UpgradeSlot {
    UpgradeKind kind;
    int maxLevel = 10;
    int costFactor = 1000;
};

struct WheelDef {
    b2Vec2 anchor;      // wheel centre at rest, chassis space (metres)
    float radius;
    float density;
    bool driven;        // driven without the 4WD upgrade
};

struct ConvexPart {
    std::vector<b2Vec2> points;  // convex, counter-clockwise, at most 8 points
    float density;
};

struct VehicleDef {
    std::string id;
    std::string name;
    std::string blurb;
    int price = 0;

    std::vector<ConvexPart> chassis;
    std::vector<WheelDef> wheels;

    // Driver. The head is its own body on a sprung neck; if it touches the
    // ground the run ends ("Driver down").
    b2Vec2 neckAnchor{};   // chassis space
    float headRadius = 0.25f;
    float headLift = 0.25f;  // head centre sits this far above the neck anchor

    // Handling at upgrade level 0.
    float motorTorque = 900.0f;       // N*m total engine torque, split across driven wheels
    float maxWheelSpeed = 45.0f;      // rad/s
    float reverseWheelSpeed = 18.0f;  // rad/s
    float brakeTorque = 1400.0f;      // N*m
    float frontDriveShare = 0.0f;     // 0 = rear-wheel drive, 1 = all wheels fully driven
    float suspHertz = 5.0f;
    float suspDamping = 0.7f;
    float suspTravelDown = 0.20f;     // metres the wheel may drop below rest
    float suspTravelUp = 0.12f;       // metres the wheel may rise above rest
    float tireFriction = 1.0f;
    float airAccel = 5.0f;            // rad/s^2 of pedal rotation while airborne
    float airMaxSpin = 4.0f;          // rad/s the pedals spin the vehicle up to
    float groundTilt = 0.15f;         // fraction of the air control available on the ground
    float fuelSeconds = 40.0f;        // full tank lasts this long
    float downforce = 0.0f;           // N per (m/s)^2
    float engineTorqueGain = 2.0f;    // torque multiplier with the engine fully upgraded
    float engineSpeedGain = 1.55f;    // top wheel speed multiplier with the engine fully upgraded
    float thrusterAccel = 0.0f;       // m/s^2 upward acceleration in chassis-local {0, 1}
    float thrusterFuelRate = 2.6f;    // extra fuel drain multiplier while thrusting
    float thrusterGain = 1.55f;       // max-level multiplier on thrusterAccel
    float nitroAccel = 11.5f;         // m/s^2 forward thrust when firing stunt-charged Nitro
    std::vector<ConvexPart> rollCage; // optional breakable roof cage polygon(s)
    int rollCageHits = 0;             // heavy roof impacts absorbed before breaking

    bool hasThrusters() const { return thrusterAccel > 0.0f; }
    bool hasRollCage() const { return !rollCage.empty() && rollCageHits > 0; }

    std::array<UpgradeSlot, 4> upgrades{};
};

// Effective handling after applying upgrades.
struct VehicleTuning {
    float motorTorque;
    float maxWheelSpeed;
    float reverseWheelSpeed;
    float brakeTorque;
    float frontDriveShare;
    float suspHertz;
    float suspDamping;
    float suspTravelDown;
    float suspTravelUp;
    float tireFriction;
    float airAccel;
    float airMaxSpin;
    float groundTilt;
    float fuelSeconds;
    float downforce;
    float thrusterAccel;
    float thrusterFuelRate;
    float nitroAccel;
    int rollCageHits;

    bool hasThrusters() const { return thrusterAccel > 0.0f; }
};

using UpgradeLevels = std::array<int, 4>;

const std::vector<StageDef>& stages();
const std::vector<VehicleDef>& vehicles();

// Returns nullptr for unknown ids.
const StageDef* findStage(const std::string& id);
const VehicleDef* findVehicle(const std::string& id);

inline constexpr UpgradeLevels kStockLevels{1, 1, 1, 1};

VehicleTuning computeTuning(const VehicleDef& def, const UpgradeLevels& levels);

// Price of buying `level` (2..maxLevel) of the given slot.
long long upgradeCost(const UpgradeSlot& slot, int level);

// Distance of level checkpoint `level` (1-based) and its one-time reward.
float levelDistance(const StageDef& stage, int level);
long long levelReward(int level);

const char* upgradeName(UpgradeKind kind);

} // namespace hr
