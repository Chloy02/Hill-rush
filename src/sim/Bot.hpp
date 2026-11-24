#pragma once

#include "sim/Session.hpp"

namespace hr {

// A simple autopilot. Used for headless soak tests, the title-screen demo and
// the --autoplay flag. It keeps the wheels down, levels out in the air and
// rocks the car free when it gets stuck.
class Bot {
public:
    InputState think(const Session& session);

    // Make the bot attempt backflips on big jumps (exercises the bonus code).
    void setStunts(bool on) { stunts_ = on; }

private:
    float stuckTimer_ = 0.0f;
    float retreatTimer_ = 0.0f;
    float retreatFromX_ = 0.0f;
    bool stunts_ = false;
};

} // namespace hr
