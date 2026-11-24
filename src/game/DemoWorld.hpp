#pragma once

#include "game/Particles.hpp"
#include "game/WorldView.hpp"
#include "sim/Bot.hpp"
#include "sim/Pose.hpp"
#include "sim/Session.hpp"

#include <memory>

namespace hr {

class App;

// A bot-driven run playing behind the menus.
class DemoWorld {
public:
    void update(float dt);
    void draw(App& app, float time) const;

private:
    std::unique_ptr<Session> session_;
    WorldView view_;
    Particles particles_;
    Bot bot_;
    VehiclePose pose_;
    VehiclePose prev_;
    float accumulator_ = 0.0f;
};

} // namespace hr
