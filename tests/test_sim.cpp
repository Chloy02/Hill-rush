#include "TestFramework.hpp"

#include "core/Math.hpp"
#include "core/Noise.hpp"
#include "sim/Defs.hpp"
#include "sim/Pickups.hpp"
#include "sim/Progress.hpp"
#include "sim/Session.hpp"
#include "sim/SimHarness.hpp"
#include "sim/Terrain.hpp"

#include <cmath>
#include <numeric>

using namespace hr;

namespace {

// Runs a session until it ends (or the step budget runs out), collecting events.
std::vector<GameEvent> runFor(Session& s, int steps, InputState input = {})
{
    std::vector<GameEvent> all;
    for (int i = 0; i < steps && !s.isOver(); ++i) {
        s.step(input);
        all.insert(all.end(), s.events().begin(), s.events().end());
        s.events().clear();
    }
    return all;
}

int countEvents(const std::vector<GameEvent>& events, EventType type)
{
    int n = 0;
    for (const GameEvent& e : events) n += e.type == type ? 1 : 0;
    return n;
}

} // namespace

TEST("noise is deterministic and bounded")
{
    Noise1D a(42), b(42), c(43);
    bool differs = false;
    for (int i = 0; i < 2000; ++i) {
        const float x = static_cast<float>(i) * 0.137f - 50.0f;
        CHECK(a(x) == b(x));
        CHECK(a(x) >= -1.0f && a(x) <= 1.0f);
        differs = differs || a(x) != c(x);
    }
    CHECK(differs);
}

TEST("noise is continuous across lattice cells")
{
    Noise1D n(7);
    for (int cell = -20; cell < 20; ++cell) {
        const float x = static_cast<float>(cell);
        CHECK_NEAR(n(x - 1e-4f), n(x + 1e-4f), 1e-3);
    }
}

TEST("wrapAngle stays in range")
{
    for (float a = -50.0f; a < 50.0f; a += 0.37f) {
        const float w = wrapAngle(a);
        CHECK(w >= -kPi && w < kPi);
        CHECK_NEAR(std::sin(w), std::sin(a), 1e-3);
        CHECK_NEAR(std::cos(w), std::cos(a), 1e-3);
    }
}

TEST("every stage has a flat, level start pad")
{
    for (const StageDef& s : stages()) {
        TerrainGenerator gen(s);
        for (float x = TerrainGenerator::kStartWallX; x <= 15.0f; x += 0.5f) CHECK_NEAR(gen.height(x), 0.0, 1e-4);
    }
}

TEST("terrain is continuous and slopes stay drivable")
{
    for (const StageDef& s : stages()) {
        TerrainGenerator gen(s);
        float steepest = 0.0f;
        for (float x = 0.0f; x < 6000.0f; x += 0.5f) {
            const float h0 = gen.height(x);
            const float h1 = gen.height(x + 0.5f);
            CHECK(std::isfinite(h0));
            steepest = std::fmax(steepest, std::fabs(h1 - h0) / 0.5f);
        }
        // 1.6 is about 58 degrees: steep, but never a cliff.
        if (steepest >= 1.6f) std::fprintf(stderr, "  stage %s steepest slope %.2f\n", s.id.c_str(), steepest);
        CHECK(steepest < 1.6f);
    }
}

TEST("start wall rises behind the start line")
{
    TerrainGenerator gen(stages().front());
    CHECK(gen.height(TerrainGenerator::kStartWallX - 10.0f) > 15.0f);
}

TEST("fuel cans never get closer together")
{
    for (const StageDef& s : stages()) {
        float lastGap = 0.0f;
        CHECK(Pickups::fuelPosition(s, 0) >= 50.0f);
        for (int i = 1; i < 60; ++i) {
            const float gap = Pickups::fuelPosition(s, i) - Pickups::fuelPosition(s, i - 1);
            CHECK(gap >= lastGap);
            CHECK(gap >= 60.0f);
            lastGap = gap;
        }
    }
}

TEST("coin clusters follow the original's value rule")
{
    const StageDef& countryside = *findStage("countryside");
    CHECK(Pickups::clusterValue(countryside, 45.0f) == 50);  // 1.2 * 45 = 54 -> 50
    CHECK(Pickups::clusterValue(countryside, 1.0f) == 5);    // never less than one coin
    CHECK(Pickups::clusterValue(countryside, 100000.0f) == Pickups::kMaxClusterValue);

    const std::vector<int> split = Pickups::splitCoins(1015);
    CHECK((split == std::vector<int>{500, 500, 5, 5, 5}));
    for (int value = 5; value <= 7000; value += 5) {
        const std::vector<int> coins = Pickups::splitCoins(value);
        CHECK(std::accumulate(coins.begin(), coins.end(), 0) == value);
        for (size_t i = 1; i < coins.size(); ++i) CHECK(coins[i] <= coins[i - 1]);  // largest first
    }
}

TEST("air time bonus matches the original's table")
{
    CHECK(airTimeBonus(1.99f) == 0);
    CHECK(airTimeBonus(2.0f) == 25);
    CHECK(airTimeBonus(2.26f) == 50);
    CHECK(airTimeBonus(2.99f) == 100);
    CHECK(airTimeBonus(3.0f) == 200);
    CHECK(airTimeBonus(3.99f) == 500);
    CHECK(airTimeBonus(4.0f) == 1000);
    CHECK(airTimeBonus(4.25f) == 1500);
    CHECK(airTimeBonus(60.0f) == 25000);
    CHECK(std::string(airTimeLabel(airTimeStep(2.1f))) == "AIR TIME");
    CHECK(std::string(airTimeLabel(airTimeStep(3.1f))) == "BIG AIR TIME");
    CHECK(std::string(airTimeLabel(airTimeStep(4.1f))) == "INSANE AIR TIME");
}

TEST("upgrade costs grow as L squared and tuning improves")
{
    for (const VehicleDef& v : vehicles()) {
        for (const UpgradeSlot& slot : v.upgrades) {
            CHECK(upgradeCost(slot, 1) == 0);
            CHECK(upgradeCost(slot, 2) == 4LL * slot.costFactor);
            long long previous = 0;
            for (int level = 2; level <= slot.maxLevel; ++level) {
                const long long cost = upgradeCost(slot, level);
                CHECK(cost > previous);
                previous = cost;
            }
        }
        UpgradeLevels maxed;
        for (size_t i = 0; i < 4; ++i) maxed[i] = v.upgrades[i].maxLevel;
        const VehicleTuning base = computeTuning(v, kStockLevels);
        const VehicleTuning top = computeTuning(v, maxed);
        CHECK(top.motorTorque > base.motorTorque);
        CHECK(top.tireFriction > base.tireFriction);
        CHECK(top.maxWheelSpeed > base.maxWheelSpeed);
    }
}

TEST("level checkpoints are spaced and rewarded like the original")
{
    const StageDef& countryside = *findStage("countryside");
    CHECK_NEAR(levelDistance(countryside, 1), 200.0, 1e-3);
    CHECK_NEAR(levelDistance(countryside, 3), 600.0, 1e-3);
    CHECK(levelReward(1) == 5000);
    CHECK(levelReward(10) == 32000);
    const StageDef& moon = *findStage("moon");
    CHECK_NEAR(levelDistance(moon, 1), 200.0, 1e-3);
    CHECK_NEAR(levelDistance(moon, 2), 500.0, 1e-3);
}

TEST("progress round-trips through text")
{
    Progress p;
    p.coins = 12345;
    p.unlockVehicle("bike");
    p.unlockStage("desert");
    p.setUpgrade("jeep", 0, 3);
    p.setUpgrade("jeep", 3, 7);
    p.setLevelsCompleted("countryside", 4);
    p.recordDistance("countryside", "jeep", 812.5f);
    p.settings.musicVolume = 0.25f;
    p.settings.fullscreen = true;
    p.selectedVehicle = "bike";

    Progress q;
    q.deserialize(p.serialize());
    CHECK(q.coins == 12345);
    CHECK(q.vehicleUnlocked("bike"));
    CHECK(!q.vehicleUnlocked("monster"));
    CHECK(q.stageUnlocked("desert"));
    CHECK(q.upgrades("jeep")[0] == 3);
    CHECK(q.upgrades("jeep")[1] == 1);
    CHECK(q.upgrades("jeep")[3] == 7);
    CHECK(q.levelsCompleted("countryside") == 4);
    CHECK(q.levelsCompleted("desert") == 0);
    CHECK_NEAR(q.best("countryside", "jeep"), 812.5, 0.01);
    CHECK_NEAR(q.settings.musicVolume, 0.25, 1e-4);
    CHECK(q.settings.fullscreen);
    CHECK(q.selectedVehicle == "bike");
}

TEST("progress ignores garbage and clamps values")
{
    Progress q;
    q.deserialize("coins=-50\nnonsense\nvehicle.jeep.upgrade.0=999\nvehicle.jeep.upgrade.1=-4\n"
                  "settings.sfx=7\nselected.vehicle=ufo\nvehicle.ufo.upgrade.0=3\n=\n");
    CHECK(q.coins == 0);
    CHECK(q.upgrades("jeep")[0] == findVehicle("jeep")->upgrades[0].maxLevel);
    CHECK(q.upgrades("jeep")[1] == 1);
    CHECK(q.upgrades("ufo") == kStockLevels);
    CHECK_NEAR(q.settings.sfxVolume, 1.0, 1e-6);
    CHECK(q.selectedVehicle == "jeep");
}

TEST("spend refuses when broke")
{
    Progress p;
    p.coins = 100;
    CHECK(!p.spend(101));
    CHECK(p.coins == 100);
    CHECK(p.spend(100));
    CHECK(p.coins == 0);
}

TEST("vehicle settles on the start pad without crashing")
{
    for (const VehicleDef& v : vehicles()) {
        Session s(stages().front(), v, kStockLevels, 0.0f);
        runFor(s, 180);
        CHECK(!s.isOver());
        CHECK(s.state() == RunState::Running);
        CHECK(std::fabs(s.vehicle().angle()) < 0.1f);
        CHECK(std::fabs(s.vehicle().position().x) < 0.5f);
        CHECK(s.vehicle().wheelsOnGround() == static_cast<int>(v.wheels.size()));
    }
}

TEST("holding gas drives forward")
{
    for (const VehicleDef& v : vehicles()) {
        Session s(stages().front(), v, kStockLevels, 0.0f);
        runFor(s, 60 * 6, {1.0f, 0.0f});
        if (s.distance() <= 20.0f) std::fprintf(stderr, "  %s only reached %.1f m\n", v.id.c_str(), s.distance());
        CHECK(s.distance() > 20.0f);
    }
}

TEST("fuel runs out when standing still")
{
    const VehicleDef& v = vehicles().front();
    Session s(stages().front(), v, kStockLevels, 0.0f);
    const auto events = runFor(s, static_cast<int>((v.fuelSeconds + 8.0f) / kFixedDt));
    CHECK(countEvents(events, EventType::LowFuel) == 1);
    CHECK(countEvents(events, EventType::OutOfFuel) == 1);
    CHECK(s.isOver());
    CHECK(s.result().reason == EndReason::OutOfFuel);
}

TEST("flipping onto the roof ends the run with Driver Down")
{
    const VehicleDef& v = vehicles().front();
    Session s(stages().front(), v, kStockLevels, 0.0f);
    runFor(s, 30);
    // Turn the whole car upside down a little above the ground; gravity does the rest.
    s.vehicleForTesting().rigidMove(kPi, {0.0f, 2.5f});
    const auto events = runFor(s, 60 * 8);
    CHECK(countEvents(events, EventType::Crash) == 1);
    CHECK(s.isOver());
    CHECK(s.result().reason == EndReason::DriverDown);
}

TEST("gas in the air does a back flip and big air pays out")
{
    const VehicleDef& v = *findVehicle("jeep");
    Session s(stages().front(), v, kStockLevels, 0.0f);
    runFor(s, 30);
    s.vehicleForTesting().rigidMove(0.0f, {0.0f, 45.0f});
    // Hold gas until most of a backflip is done, then level out with the brake.
    std::vector<GameEvent> events;
    for (int i = 0; i < 60 * 6 && !s.isOver(); ++i) {
        InputState in;
        const int flips = s.result().backFlips;
        const float angle = wrapAngle(s.vehicle().angle());
        if (flips == 0) in.gas = 1.0f;
        else if (angle > 0.15f) in.brake = 1.0f;
        else if (angle < -0.15f) in.gas = 1.0f;
        s.step(in);
        events.insert(events.end(), s.events().begin(), s.events().end());
        s.events().clear();
        if (!s.airborne() && s.time() > 2.0f) break;
    }
    CHECK(s.result().backFlips >= 1);
    CHECK(countEvents(events, EventType::Flip) >= 1);
    CHECK(s.result().bonusCoins >= Session::kFlipBonus);
}

TEST("level checkpoints pay out only once per stage")
{
    const StageDef& stage = *findStage("countryside");
    const VehicleDef& v = *findVehicle("jeep");
    UpgradeLevels strong{9, 8, 10, 8};
    {
        Session s(stage, v, strong, 0.0f, 0);
        const auto events = runFor(s, 60 * 40, {1.0f, 0.0f});
        // Bot-free full throttle may crash early; only check what was reached.
        const int levels = countEvents(events, EventType::LevelUp);
        CHECK(levels == s.result().levelsReached);
        CHECK(s.levelsCompleted() == levels);
    }
    {
        // Already completed 50 levels: nothing nearby should pay again.
        Session s(stage, v, strong, 0.0f, 50);
        const auto events = runFor(s, 60 * 20, {1.0f, 0.0f});
        CHECK(countEvents(events, EventType::LevelUp) == 0);
        CHECK(s.result().levelCoins == 0);
    }
}

TEST("bot run is finite and makes progress on every stage")
{
    for (const StageDef& stage : stages()) {
        SimOptions o;
        o.stage = stage.id;
        o.vehicle = "jeep";
        o.maxSeconds = 120.0f;
        const SimReport r = runHeadless(o);
        std::fprintf(stderr, "  %s\n", toJson(o, r).c_str());
        CHECK(r.finite);
        CHECK(r.result.distance > 100.0f);
    }
}

TEST("easy difficulty leaves every stage exactly as it was")
{
    for (const StageDef& s : stages()) {
        const StageDef e = applyDifficulty(s, Difficulty::Easy);
        TerrainGenerator a(s), b(e);
        for (float x = -20.0f; x < 3000.0f; x += 0.5f) CHECK(a.height(x) == b.height(x));
        CHECK(e.coinRate == s.coinRate);
        CHECK(e.fuelGapGrowth == s.fuelGapGrowth);
        CHECK(e.fuelTankScale == 1.0f);
        CHECK(e.levelRewardScale == 1.0f);
    }
}

TEST("harder difficulties are rougher, give less fuel and pay more")
{
    for (const StageDef& s : stages()) {
        const StageDef e = applyDifficulty(s, Difficulty::Easy);
        const StageDef m = applyDifficulty(s, Difficulty::Medium);
        const StageDef h = applyDifficulty(s, Difficulty::Hard);
        CHECK(m.extraBumps > e.extraBumps && h.extraBumps > m.extraBumps);
        CHECK(m.fuelTankScale < 1.0f && h.fuelTankScale < m.fuelTankScale);
        CHECK(m.fuelGapGrowth > e.fuelGapGrowth && h.fuelGapGrowth > m.fuelGapGrowth);
        CHECK(m.coinRate > e.coinRate && h.coinRate > m.coinRate);
        CHECK(h.levelRewardScale > m.levelRewardScale && m.levelRewardScale > 1.0f);
        // Harder modes keep the slope limits, so they add no impassable walls.
        CHECK(h.maxSlopeEnd == s.maxSlopeEnd && h.maxSlopeStart == s.maxSlopeStart);
        TerrainGenerator gen(h);
        float steepest = 0.0f;
        double roughness[2] = {0.0, 0.0};
        TerrainGenerator easyGen(e);
        for (float x = 0.0f; x < 6000.0f; x += 0.5f) {
            const float d = gen.height(x + 0.5f) - gen.height(x);
            steepest = std::fmax(steepest, std::fabs(d) / 0.5f);
            roughness[0] += std::fabs(easyGen.height(x + 0.5f) - easyGen.height(x));
            roughness[1] += std::fabs(d);
        }
        CHECK(steepest <= std::tan(degToRad(h.maxSlopeEnd)) + 1e-3f);
        CHECK(roughness[1] > roughness[0] * 1.05);  // the ground really is bumpier
    }
}

TEST("difficulty setting and per-difficulty records round-trip")
{
    Progress p;
    p.settings.difficulty = Difficulty::Hard;
    p.recordDistance("countryside", "jeep", 900.0f, Difficulty::Easy);
    p.recordDistance("countryside", "jeep", 300.0f, Difficulty::Hard);
    p.setLevelsCompleted("countryside", 4, Difficulty::Easy);
    p.setLevelsCompleted("countryside", 1, Difficulty::Hard);

    Progress q;
    q.deserialize(p.serialize());
    CHECK(q.settings.difficulty == Difficulty::Hard);
    CHECK_NEAR(q.best("countryside", "jeep", Difficulty::Easy), 900.0, 0.01);
    CHECK_NEAR(q.best("countryside", "jeep", Difficulty::Hard), 300.0, 0.01);
    CHECK_NEAR(q.best("countryside", "jeep", Difficulty::Medium), 0.0, 0.01);
    CHECK(q.levelsCompleted("countryside", Difficulty::Easy) == 4);
    CHECK(q.levelsCompleted("countryside", Difficulty::Hard) == 1);
    CHECK(q.levelsCompleted("countryside", Difficulty::Medium) == 0);
}

TEST("the online player name is saved")
{
    Progress p;
    CHECK(p.serialize().find("settings.name") == std::string::npos);  // not chosen yet
    p.settings.playerName = "BIG AL";
    Progress q;
    q.deserialize(p.serialize());
    CHECK(q.settings.playerName == "BIG AL");
}

TEST("saves from before difficulties load as easy progress")
{
    Progress q;
    q.deserialize("coins=500\nbest.countryside|jeep=1620.1\nstage.countryside.levels=8\n");
    CHECK(q.settings.difficulty == Difficulty::Easy);
    CHECK_NEAR(q.best("countryside", "jeep", Difficulty::Easy), 1620.1, 0.01);
    CHECK(q.levelsCompleted("countryside", Difficulty::Easy) == 8);
    CHECK(q.levelsCompleted("countryside", Difficulty::Hard) == 0);
}

TEST("hard mode pays more per level and drains fuel faster")
{
    const StageDef& base = *findStage("countryside");
    const VehicleDef& jeep = *findVehicle("jeep");
    Session easy(applyDifficulty(base, Difficulty::Easy), jeep, kStockLevels, 0.0f);
    Session hard(applyDifficulty(base, Difficulty::Hard), jeep, kStockLevels, 0.0f);
    // Idle for 20 s: the hard tank (70%) empties faster.
    for (int i = 0; i < 60 * 20; ++i) {
        easy.step({});
        hard.step({});
    }
    CHECK(hard.fuel() < easy.fuel());
    CHECK_NEAR(easy.fuel(), 1.0 - 20.0 / 40.0, 0.02);
    CHECK_NEAR(hard.fuel(), 1.0 - 20.0 / (40.0 * difficultyDef(Difficulty::Hard).fuelTankScale), 0.02);
}
