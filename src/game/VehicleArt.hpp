#pragma once

#include "sim/Defs.hpp"
#include "sim/Pose.hpp"

#include <box2d/math_functions.h>

namespace hr {

// Draws vehicle and driver in world space (call inside BeginMode2D).
void drawVehicle(const VehicleDef& def, const VehiclePose& pose, float time);

// Exhaust outlet in chassis space and the direction smoke leaves it.
b2Vec2 exhaustPoint(const VehicleDef& def);
b2Vec2 exhaustDirection(const VehicleDef& def);

} // namespace hr
