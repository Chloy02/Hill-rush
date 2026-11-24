#pragma once

#include "sim/Session.hpp"

#include <string>

namespace hr {

struct SimOptions {
    std::string stage = "countryside";
    std::string vehicle = "jeep";
    UpgradeLevels upgrades = kStockLevels;
    Difficulty difficulty = Difficulty::Easy;
    float maxSeconds = 600.0f;
    bool stunts = false;
    bool verbose = false;  // print a telemetry line every simulated second
};

struct SimReport {
    RunResult result;
    float maxSpeed = 0.0f;
    float maxAngularSpeed = 0.0f;
    int fuelPickups = 0;
    int airTimeBonuses = 0;
    bool finite = true;      // no NaN/inf seen
    bool timedOut = false;
    double wallSeconds = 0.0;
};

// Runs one bot-driven session without any rendering.
SimReport runHeadless(const SimOptions& options);

std::string toJson(const SimOptions& options, const SimReport& report);

// Puts a vehicle in front of a straight ramp of `angleDeg` (after `runUp`
// metres of flat ground) and reports whether it reaches the top within 25 s.
// Used to calibrate terrain steepness against vehicle capability.
bool climbTest(const VehicleDef& vehicle, const UpgradeLevels& upgrades, float angleDeg, float runUp,
               float groundFriction = 0.95f, float gravity = 9.8f);

} // namespace hr
