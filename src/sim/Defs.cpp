#include "sim/Defs.hpp"

#include <algorithm>
#include <cmath>

namespace hr {

namespace {

std::vector<StageDef> makeStages()
{
    std::vector<StageDef> list;

    {
        StageDef s;
        s.id = "countryside";
        s.name = "Countryside";
        s.blurb = "Rolling green hills. The perfect place to learn.";
        s.price = 0;
        s.gravity = 9.8f;
        s.groundFriction = 0.95f;
        s.seed = 0xC0047A1u;
        s.rampDistance = 1800.0f;
        s.octaves = {{{130.0f, 9.0f, 20.0f}, {36.0f, 4.5f, 11.0f}, {10.0f, 0.45f, 1.5f}}};
        s.steepStart = 1.0f;
        s.steepEnd = 1.7f;
        s.maxSlopeStart = 34.0f;
        s.maxSlopeEnd = 46.0f;
        s.fuelFirst = 90.0f;
        s.fuelGap = 90.0f;
        s.fuelGapGrowth = 2.0f;
        s.coinRate = 1.2f;
        s.levelSpacing = 200.0f;
        list.push_back(s);
    }
    {
        StageDef s;
        s.id = "desert";
        s.name = "Desert";
        s.blurb = "Grippy sand, towering dunes and near-vertical climbs.";
        s.price = 35000;
        s.gravity = 9.8f;
        s.groundFriction = 1.1f;
        s.rollingResistance = 0.035f;
        s.seed = 0xDE5E47u;
        s.rampDistance = 2400.0f;
        s.octaves = {{{175.0f, 11.0f, 25.0f}, {52.0f, 6.0f, 15.0f}, {14.0f, 0.2f, 0.6f}}};
        s.steepStart = 1.4f;
        s.steepEnd = 2.2f;
        s.maxSlopeStart = 36.0f;
        s.maxSlopeEnd = 48.0f;
        s.fuelFirst = 90.0f;
        s.fuelGap = 90.0f;
        s.fuelGapGrowth = 4.0f;
        s.coinRate = 1.05f;
        s.levelSpacing = 200.0f;
        list.push_back(s);
    }
    {
        StageDef s;
        s.id = "arctic";
        s.name = "Arctic";
        s.blurb = "Snowy hills with treacherous patches of sheer ice.";
        s.price = 75000;
        s.gravity = 9.8f;
        s.groundFriction = 0.5f;
        s.seed = 0xA4C71Cu;
        s.rampDistance = 2400.0f;
        s.octaves = {{{135.0f, 8.0f, 18.0f}, {38.0f, 4.0f, 9.0f}, {10.0f, 0.4f, 1.1f}}};
        s.steepStart = 1.0f;
        s.steepEnd = 1.4f;
        s.feature = TerrainFeature::IcePatches;
        s.iceFriction = 0.12f;
        s.maxSlopeStart = 30.0f;
        s.maxSlopeEnd = 42.0f;
        s.fuelFirst = 90.0f;
        s.fuelGap = 90.0f;
        s.fuelGapGrowth = 3.0f;
        s.coinRate = 1.9f;
        s.levelSpacing = 200.0f;
        list.push_back(s);
    }
    {
        StageDef s;
        s.id = "highlands";
        s.name = "Highlands";
        s.blurb = "Huge, fast hills. Fuel is scarce - keep your speed up.";
        s.price = 125000;
        s.gravity = 9.8f;
        s.groundFriction = 1.0f;
        s.seed = 0x416A1Du;
        s.rampDistance = 2200.0f;
        s.octaves = {{{190.0f, 12.0f, 30.0f}, {48.0f, 5.0f, 11.0f}, {12.0f, 0.25f, 0.8f}}};
        s.steepStart = 1.1f;
        s.steepEnd = 1.65f;
        s.maxSlopeStart = 34.0f;
        s.maxSlopeEnd = 46.0f;
        s.fuelFirst = 150.0f;
        s.fuelGap = 150.0f;
        s.fuelGapGrowth = 30.0f;
        s.coinRate = 1.15f;
        s.levelSpacing = 400.0f;
        list.push_back(s);
    }
    {
        StageDef s;
        s.id = "moon";
        s.name = "Moon";
        s.blurb = "Craters everywhere and barely any gravity.";
        s.price = 175000;
        s.gravity = 1.96f;
        s.groundFriction = 0.85f;
        s.seed = 0x300Du;
        s.rampDistance = 2600.0f;
        s.octaves = {{{160.0f, 6.0f, 14.0f}, {46.0f, 2.5f, 6.0f}, {11.0f, 0.15f, 0.45f}}};
        s.feature = TerrainFeature::Craters;
        s.maxSlopeStart = 38.0f;
        s.maxSlopeEnd = 50.0f;
        s.fuelFirst = 90.0f;
        s.fuelGap = 90.0f;
        s.fuelGapGrowth = 2.0f;
        s.coinRate = 1.9f;
        s.levelSpacing = 300.0f;
        s.levelOffset = -100.0f;
        list.push_back(s);
    }
    {
        StageDef s;
        s.id = "mars";
        s.name = "Mars";
        s.blurb = "Red rocks, low gravity and long gaps between fuel.";
        s.price = 250000;
        s.gravity = 4.4f;
        s.groundFriction = 0.9f;
        s.seed = 0x3A45u;
        s.rampDistance = 2600.0f;
        s.octaves = {{{125.0f, 8.0f, 19.0f}, {32.0f, 3.5f, 8.5f}, {8.5f, 0.35f, 1.1f}}};
        s.steepStart = 1.0f;
        s.steepEnd = 1.35f;
        s.maxSlopeStart = 34.0f;
        s.maxSlopeEnd = 46.0f;
        s.fuelFirst = 150.0f;
        s.fuelGap = 135.0f;
        s.fuelGapGrowth = 10.0f;
        s.coinRate = 1.5f;
        s.levelSpacing = 300.0f;
        s.levelOffset = -100.0f;
        list.push_back(s);
    }
    return list;
}

UpgradeSlot slot(UpgradeKind kind, int maxLevel, int costFactor = 1000)
{
    UpgradeSlot u;
    u.kind = kind;
    u.maxLevel = maxLevel;
    u.costFactor = costFactor;
    return u;
}

std::vector<VehicleDef> makeVehicles()
{
    std::vector<VehicleDef> list;

    {
        VehicleDef v;
        v.id = "jeep";
        v.name = "Jeep";
        v.blurb = "The trusty all-rounder. Upgrade 4WD to climb anything.";
        v.price = 0;
        v.chassis = {
            {{{-1.45f, -0.22f}, {1.30f, -0.22f}, {1.48f, 0.02f}, {1.40f, 0.24f}, {0.55f, 0.30f}, {-1.45f, 0.30f}}, 135.0f},
            // roll bar behind the driver
            {{{-0.98f, 0.30f}, {-0.86f, 0.30f}, {-0.80f, 0.95f}, {-0.92f, 0.95f}}, 30.0f},
        };
        v.wheels = {
            {{-0.98f, -0.40f}, 0.42f, 60.0f, true},
            {{0.98f, -0.40f}, 0.42f, 60.0f, false},
        };
        v.neckAnchor = {-0.30f, 0.82f};
        v.headRadius = 0.27f;
        v.headLift = 0.27f;
        v.motorTorque = 520.0f;
        v.engineTorqueGain = 2.0f;
        v.maxWheelSpeed = 42.0f;
        v.reverseWheelSpeed = 16.0f;
        v.brakeTorque = 1000.0f;
        v.frontDriveShare = 0.0f;
        v.suspHertz = 4.5f;
        v.suspDamping = 0.65f;
        v.suspTravelDown = 0.20f;
        v.suspTravelUp = 0.14f;
        v.tireFriction = 1.0f;
        v.airAccel = 5.0f;
        v.airMaxSpin = 4.2f;
        v.fuelSeconds = 40.0f;
        v.upgrades = {slot(UpgradeKind::Engine, 13), slot(UpgradeKind::Suspension, 14),
                      slot(UpgradeKind::Tires, 16), slot(UpgradeKind::FourWheelDrive, 10)};
        list.push_back(v);
    }
    {
        VehicleDef v;
        v.id = "bike";
        v.name = "Dirt Bike";
        v.blurb = "Light and twitchy. Flips in a heartbeat - for better or worse.";
        v.price = 75000;
        v.chassis = {
            {{{-0.70f, -0.10f}, {0.55f, -0.10f}, {0.82f, 0.20f}, {0.10f, 0.42f}, {-0.70f, 0.32f}}, 90.0f},
            // rider's body
            {{{-0.32f, 0.32f}, {0.05f, 0.32f}, {0.22f, 0.92f}, {-0.10f, 0.92f}}, 70.0f},
        };
        v.wheels = {
            {{-0.80f, -0.30f}, 0.40f, 32.0f, true},
            {{0.86f, -0.30f}, 0.40f, 32.0f, false},
        };
        v.neckAnchor = {0.10f, 0.95f};
        v.headRadius = 0.24f;
        v.headLift = 0.24f;
        v.motorTorque = 190.0f;
        v.engineTorqueGain = 1.45f;
        v.maxWheelSpeed = 50.0f;
        v.reverseWheelSpeed = 14.0f;
        v.brakeTorque = 420.0f;
        v.suspHertz = 4.0f;
        v.suspDamping = 0.6f;
        v.suspTravelDown = 0.22f;
        v.suspTravelUp = 0.16f;
        v.tireFriction = 1.05f;
        v.airAccel = 6.5f;
        v.airMaxSpin = 5.5f;
        v.fuelSeconds = 40.0f;
        v.upgrades = {slot(UpgradeKind::Engine, 12, 1500), slot(UpgradeKind::Suspension, 14),
                      slot(UpgradeKind::Tires, 16), slot(UpgradeKind::AirControl, 10, 3000)};
        list.push_back(v);
    }
    {
        VehicleDef v;
        v.id = "monster";
        v.name = "Monster Truck";
        v.blurb = "Giant tyres, giant suspension. Rolls over anything.";
        v.price = 100000;
        v.chassis = {
            {{{-1.55f, -0.05f}, {1.45f, -0.05f}, {1.65f, 0.22f}, {1.50f, 0.52f}, {-1.55f, 0.52f}}, 95.0f},
            // roll cage around the cab
            {{{-0.85f, 0.52f}, {-0.72f, 0.52f}, {-0.66f, 1.22f}, {-0.79f, 1.22f}}, 30.0f},
        };
        v.wheels = {
            {{-1.22f, -0.60f}, 0.78f, 24.0f, true},
            {{1.22f, -0.60f}, 0.78f, 24.0f, false},
        };
        v.neckAnchor = {-0.20f, 0.98f};
        v.headRadius = 0.27f;
        v.headLift = 0.27f;
        v.motorTorque = 1000.0f;
        v.engineTorqueGain = 1.8f;
        v.maxWheelSpeed = 30.0f;
        v.reverseWheelSpeed = 12.0f;
        v.brakeTorque = 1800.0f;
        v.frontDriveShare = 0.0f;
        v.suspHertz = 3.2f;
        v.suspDamping = 0.5f;
        v.suspTravelDown = 0.38f;
        v.suspTravelUp = 0.26f;
        v.tireFriction = 1.0f;
        v.airAccel = 4.2f;
        v.airMaxSpin = 3.6f;
        v.fuelSeconds = 40.0f;
        v.upgrades = {slot(UpgradeKind::Engine, 14), slot(UpgradeKind::Suspension, 16),
                      slot(UpgradeKind::Tires, 18), slot(UpgradeKind::FourWheelDrive, 10)};
        list.push_back(v);
    }
    {
        VehicleDef v;
        v.id = "tractor";
        v.name = "Tractor";
        v.blurb = "Slow, stubborn and thrifty. That huge rear wheel grips like a goat.";
        v.price = 100000;
        v.chassis = {
            {{{-1.20f, -0.12f}, {1.15f, -0.12f}, {1.30f, 0.10f}, {1.20f, 0.42f}, {-1.20f, 0.42f}}, 150.0f},
            // cab frame
            {{{-1.05f, 0.42f}, {-0.92f, 0.42f}, {-0.86f, 1.25f}, {-0.99f, 1.25f}}, 30.0f},
        };
        v.wheels = {
            {{-0.80f, -0.32f}, 0.74f, 32.0f, true},
            {{1.00f, -0.62f}, 0.44f, 32.0f, false},
        };
        v.neckAnchor = {-0.45f, 1.02f};
        v.headRadius = 0.26f;
        v.headLift = 0.26f;
        v.motorTorque = 900.0f;
        v.engineTorqueGain = 1.1f;
        v.engineSpeedGain = 1.7f;
        v.maxWheelSpeed = 22.0f;
        v.reverseWheelSpeed = 10.0f;
        v.brakeTorque = 2000.0f;
        v.suspHertz = 4.0f;
        v.suspDamping = 0.7f;
        v.suspTravelDown = 0.18f;
        v.suspTravelUp = 0.12f;
        v.tireFriction = 1.15f;
        v.airAccel = 3.6f;
        v.airMaxSpin = 3.0f;
        v.fuelSeconds = 60.0f;
        v.upgrades = {slot(UpgradeKind::Engine, 16), slot(UpgradeKind::Suspension, 15),
                      slot(UpgradeKind::Tires, 15), slot(UpgradeKind::FuelTank, 20)};
        list.push_back(v);
    }
    {
        VehicleDef v;
        v.id = "racecar";
        v.name = "Race Car";
        v.blurb = "Blistering speed and low-slung grip. Hates bumps.";
        v.price = 250000;
        v.chassis = {
            {{{-1.75f, -0.14f}, {1.65f, -0.14f}, {1.90f, -0.02f}, {1.60f, 0.16f}, {0.30f, 0.26f}, {-1.75f, 0.26f}}, 115.0f},
            // rear wing pillar
            {{{-1.75f, 0.26f}, {-1.55f, 0.26f}, {-1.50f, 0.62f}, {-1.75f, 0.62f}}, 30.0f},
        };
        v.wheels = {
            {{-1.22f, -0.22f}, 0.38f, 50.0f, true},
            {{1.28f, -0.22f}, 0.38f, 50.0f, false},
        };
        v.neckAnchor = {-0.25f, 0.30f};
        v.headRadius = 0.23f;
        v.headLift = 0.25f;
        v.motorTorque = 460.0f;
        v.engineTorqueGain = 1.6f;
        v.maxWheelSpeed = 68.0f;
        v.reverseWheelSpeed = 16.0f;
        v.brakeTorque = 1200.0f;
        v.suspHertz = 6.5f;
        v.suspDamping = 0.8f;
        v.suspTravelDown = 0.12f;
        v.suspTravelUp = 0.08f;
        v.tireFriction = 1.1f;
        v.airAccel = 4.6f;
        v.airMaxSpin = 4.0f;
        v.fuelSeconds = 40.0f;
        v.downforce = 0.0f;
        v.upgrades = {slot(UpgradeKind::Engine, 12, 1500), slot(UpgradeKind::Suspension, 14),
                      slot(UpgradeKind::Tires, 16), slot(UpgradeKind::Downforce, 10, 3000)};
        list.push_back(v);
    }
    return list;
}

} // namespace

const std::array<DifficultyDef, 3>& difficulties()
{
    // Easy is the original tuning. Medium and Hard add rough ground (jumps,
    // bounces, flips), cut the fuel supply and pay more for every metre to
    // make the extra risk worth it. Values were tuned with bot runs.
    static const std::array<DifficultyDef, 3> list = {{
        {Difficulty::Easy, "easy", "Easy", "Smooth hills and plenty of fuel.", 0.0f, 1.0f, 1.0f, 1.0f},
        {Difficulty::Medium, "medium", "Medium", "Rough, bumpy ground and less fuel. Coins x1.5", 1.0f, 0.8f, 2.0f,
         1.5f},
        {Difficulty::Hard, "hard", "Hard", "Wild, jumpy ground and scarce fuel. Coins x2", 2.0f, 0.65f, 3.0f, 2.0f},
    }};
    return list;
}

const DifficultyDef& difficultyDef(Difficulty d)
{
    return difficulties()[static_cast<size_t>(d)];
}

Difficulty difficultyFromKey(const std::string& key)
{
    for (const DifficultyDef& d : difficulties()) {
        if (key == d.key) return d.id;
    }
    return Difficulty::Easy;
}

StageDef applyDifficulty(const StageDef& base, Difficulty difficulty)
{
    StageDef s = base;
    s.difficulty = difficulty;
    if (difficulty == Difficulty::Easy) return s;

    const DifficultyDef& d = difficultyDef(difficulty);
    s.extraBumps = d.extraBumps;
    s.fuelTankScale = d.fuelTankScale;
    s.fuelGapGrowth *= d.fuelGrowthScale;
    s.coinRate *= d.rewardScale;
    s.levelRewardScale = d.rewardScale;
    return s;
}

const std::vector<StageDef>& stages()
{
    static const std::vector<StageDef> list = makeStages();
    return list;
}

const std::vector<VehicleDef>& vehicles()
{
    static const std::vector<VehicleDef> list = makeVehicles();
    return list;
}

const StageDef* findStage(const std::string& id)
{
    for (const StageDef& s : stages()) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const VehicleDef* findVehicle(const std::string& id)
{
    for (const VehicleDef& v : vehicles()) {
        if (v.id == id) return &v;
    }
    return nullptr;
}

VehicleTuning computeTuning(const VehicleDef& def, const UpgradeLevels& levels)
{
    VehicleTuning t;
    t.motorTorque = def.motorTorque;
    t.maxWheelSpeed = def.maxWheelSpeed;
    t.reverseWheelSpeed = def.reverseWheelSpeed;
    t.brakeTorque = def.brakeTorque;
    t.frontDriveShare = def.frontDriveShare;
    t.suspHertz = def.suspHertz;
    t.suspDamping = def.suspDamping;
    t.suspTravelDown = def.suspTravelDown;
    t.suspTravelUp = def.suspTravelUp;
    t.tireFriction = def.tireFriction;
    t.airAccel = def.airAccel;
    t.airMaxSpin = def.airMaxSpin;
    t.groundTilt = def.groundTilt;
    t.fuelSeconds = def.fuelSeconds;
    t.downforce = def.downforce;

    for (size_t i = 0; i < def.upgrades.size(); ++i) {
        const UpgradeSlot& slot = def.upgrades[i];
        const int level = std::clamp(levels[i], 1, slot.maxLevel);
        // Effects are defined for a fully upgraded slot; p is the fraction of the way there.
        const float p = slot.maxLevel > 1 ? static_cast<float>(level - 1) / static_cast<float>(slot.maxLevel - 1) : 0.0f;
        if (p <= 0.0f) continue;
        switch (slot.kind) {
        case UpgradeKind::Engine:
            t.motorTorque *= 1.0f + (def.engineTorqueGain - 1.0f) * p;
            t.maxWheelSpeed *= 1.0f + (def.engineSpeedGain - 1.0f) * p;
            t.brakeTorque *= 1.0f + 0.4f * p;
            break;
        case UpgradeKind::Suspension:
            t.suspHertz *= 1.0f + 0.25f * p;
            t.suspDamping = std::fmin(1.0f, t.suspDamping + 0.25f * p);
            t.suspTravelDown *= 1.0f + 0.35f * p;
            t.suspTravelUp *= 1.0f + 0.35f * p;
            break;
        case UpgradeKind::Tires:
            t.tireFriction += 0.75f * p;
            break;
        case UpgradeKind::FourWheelDrive:
            t.frontDriveShare = std::fmin(1.0f, t.frontDriveShare + p);
            break;
        case UpgradeKind::FuelTank:
            t.fuelSeconds *= 1.0f + 1.0f * p;
            break;
        case UpgradeKind::AirControl:
            t.airAccel *= 1.0f + 1.0f * p;
            t.airMaxSpin *= 1.0f + 0.5f * p;
            break;
        case UpgradeKind::Downforce:
            t.downforce += 8.0f * p;
            break;
        }
    }
    return t;
}

long long upgradeCost(const UpgradeSlot& slot, int level)
{
    if (level < 2) return 0;
    return static_cast<long long>(slot.costFactor) * level * level;
}

float levelDistance(const StageDef& stage, int level)
{
    return stage.levelSpacing * static_cast<float>(level) + stage.levelOffset;
}

long long levelReward(int level)
{
    return 3000LL * level + 2000LL;
}

const char* upgradeName(UpgradeKind kind)
{
    switch (kind) {
    case UpgradeKind::Engine: return "Engine";
    case UpgradeKind::Suspension: return "Suspension";
    case UpgradeKind::Tires: return "Tires";
    case UpgradeKind::FourWheelDrive: return "4WD";
    case UpgradeKind::FuelTank: return "Fuel Tank";
    case UpgradeKind::AirControl: return "Air Control";
    case UpgradeKind::Downforce: return "Downforce";
    }
    return "?";
}

} // namespace hr
