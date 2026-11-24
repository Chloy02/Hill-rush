#include "sim/SimHarness.hpp"

#include "sim/Bot.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>

namespace hr {

SimReport runHeadless(const SimOptions& options)
{
    SimReport report;
    const StageDef* stage = findStage(options.stage);
    const VehicleDef* vehicle = findVehicle(options.vehicle);
    if (!stage || !vehicle) {
        report.finite = false;
        return report;
    }

    const auto start = std::chrono::steady_clock::now();
    Session session(applyDifficulty(*stage, options.difficulty), *vehicle, options.upgrades, 0.0f);
    Bot bot;
    bot.setStunts(options.stunts);

    const int maxSteps = static_cast<int>(options.maxSeconds / kFixedDt);
    int step = 0;
    for (; step < maxSteps && !session.isOver(); ++step) {
        session.step(bot.think(session));

        const Vehicle& v = session.vehicle();
        const b2Vec2 p = v.position();
        const b2Vec2 vel = v.velocity();
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(vel.x)) report.finite = false;
        report.maxSpeed = std::fmax(report.maxSpeed, v.speed());
        report.maxAngularSpeed = std::fmax(report.maxAngularSpeed, std::fabs(v.angularVelocity()));

        for (const GameEvent& e : session.events()) {
            if (e.type == EventType::Fuel) report.fuelPickups++;
            if (e.type == EventType::AirTime) report.airTimeBonuses++;
        }
        session.events().clear();

        if (options.verbose && step % 60 == 0) {
            std::fprintf(stderr, "t=%6.1f x=%8.1f y=%7.1f v=%5.1f ang=%6.2f fuel=%4.2f coins=%lld air=%d\n",
                         session.time(), p.x, p.y, v.speed(), v.angle(), session.fuel(), session.coins(),
                         v.airborne() ? 1 : 0);
        }
    }
    report.timedOut = !session.isOver();
    report.result = session.result();
    report.result.distance = session.distance();
    report.wallSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return report;
}

bool climbTest(const VehicleDef& vehicle, const UpgradeLevels& upgrades, float angleDeg, float runUp,
               float groundFriction, float gravity)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = {0.0f, -gravity};
    worldDef.frictionCallback = [](float a, int, float b, int) { return a * b; };
    worldDef.enableSleep = false;
    const b2WorldId world = b2CreateWorld(&worldDef);

    // Flat approach, ramp of the given angle up 14 m, then a flat top.
    const float slope = std::tan(angleDeg * 3.14159265f / 180.0f);
    const float rampLength = 14.0f / slope;
    std::vector<b2Vec2> points;
    for (float x = rampLength + 40.0f; x >= -runUp - 20.0f; x -= 0.5f) {
        const float y = x <= 0.0f ? 0.0f : (x >= rampLength ? 14.0f : x * slope);
        points.push_back({x, y});
    }
    b2BodyDef groundDef = b2DefaultBodyDef();
    const b2BodyId ground = b2CreateBody(world, &groundDef);
    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.friction = groundFriction;
    b2ChainDef chainDef = b2DefaultChainDef();
    chainDef.points = points.data();
    chainDef.count = static_cast<int>(points.size());
    chainDef.materials = &material;
    chainDef.materialCount = 1;
    chainDef.filter.categoryBits = kCategoryGround;
    b2CreateChain(ground, &chainDef);

    float lowest = 0.0f;
    for (const WheelDef& w : vehicle.wheels) lowest = std::fmin(lowest, w.anchor.y - w.radius);
    // Start with the front wheel just short of the ramp foot.
    const float frontX = vehicle.wheels.back().anchor.x + vehicle.wheels.back().radius;
    bool success = false;
    {
        Vehicle v(world, vehicle, computeTuning(vehicle, upgrades), {-runUp - frontX - 0.3f, -lowest + 0.05f});
        for (int step = 0; step < 25 * 60; ++step) {
            // Same wheelie control as the bot: ease off when the nose lifts too far.
            const float x = v.position().x;
            const float groundAngle = x > 0.0f && x < rampLength ? std::atan(slope) : 0.0f;
            const float lift = std::remainder(v.angle() - groundAngle, 6.2831853f) + v.angularVelocity() * 0.3f;
            const float gas = lift > 0.5f ? 0.0f : 1.0f;
            const float brake = lift > 0.85f ? 1.0f : 0.0f;
            v.applyControls(gas, brake, 0.0f, true, false, kFixedDt);
            b2World_Step(world, kFixedDt, kSubSteps);
            v.updateContacts(kFixedDt);
            if (v.position().x > rampLength + 2.0f && v.position().y > 13.0f) {
                success = true;
                break;
            }
            if (v.headTouchingGround()) break;
        }
    }
    b2DestroyWorld(world);
    return success;
}

std::string toJson(const SimOptions& o, const SimReport& r)
{
    std::ostringstream out;
    out << "{\"stage\":\"" << o.stage << "\",\"difficulty\":\"" << difficultyDef(o.difficulty).key
        << "\",\"vehicle\":\"" << o.vehicle << "\",\"upgrades\":["
        << o.upgrades[0] << "," << o.upgrades[1] << "," << o.upgrades[2] << "," << o.upgrades[3] << "]"
        << ",\"end\":\"" << endReasonText(r.result.reason) << "\""
        << ",\"timedOut\":" << (r.timedOut ? "true" : "false")
        << ",\"distance\":" << static_cast<int>(r.result.distance)
        << ",\"time\":" << static_cast<int>(r.result.duration)
        << ",\"coins\":" << r.result.pickupCoins << ",\"bonus\":" << r.result.bonusCoins
        << ",\"levelCoins\":" << r.result.levelCoins << ",\"levels\":" << r.result.levelsReached
        << ",\"flips\":" << r.result.flips() << ",\"neckFlips\":" << r.result.neckFlips
        << ",\"fuelCans\":" << r.fuelPickups
        << ",\"airBonuses\":" << r.airTimeBonuses
        << ",\"maxSpeed\":" << static_cast<int>(r.maxSpeed * 3.6f)
        << ",\"maxSpin\":" << r.maxAngularSpeed
        << ",\"finite\":" << (r.finite ? "true" : "false")
        << ",\"wallMs\":" << static_cast<int>(r.wallSeconds * 1000.0) << "}";
    return out.str();
}

} // namespace hr
