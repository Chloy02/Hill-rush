#pragma once

#include "core/Noise.hpp"
#include "game/Assets.hpp"
#include "game/Palette.hpp"
#include "game/Particles.hpp"
#include "game/VehicleArt.hpp"
#include "sim/Session.hpp"

#include <raylib.h>

#include <vector>

namespace hr {

// Another player's car in an online race.
struct OtherCar {
    const VehicleDef* def = nullptr;
    VehiclePose pose;
};

// Draws a running session: sky, parallax background, terrain, pickups,
// markers, the vehicle and particles. Owns the camera.
class WorldView {
public:
    void reset(const StageDef& stage, b2Vec2 focus);
    void update(float dt, b2Vec2 focus, b2Vec2 velocity);

    // `others` (online races) are drawn just behind the session's own car.
    void draw(const Session& session, const VehiclePose& pose, const Particles& particles, const Assets& assets,
              float time, bool debugPhysics, const std::vector<OtherCar>& others = {}) const;

    Camera2D camera() const;
    Vector2 toScreen(b2Vec2 world) const;
    float pixelsPerMetre() const { return ppm_; }
    b2Vec2 cameraCentre() const { return centre_; }

    // Shakes the camera for `seconds` (crashes, hard landings).
    void shake(float strength, float seconds);

private:
    void drawSky(const Palette& pal, float time) const;
    void drawHills(const Palette& pal, int layer) const;
    void drawTerrain(const Session& session, const Palette& pal, const Assets& assets) const;
    void drawProps(const Session& session, const Palette& pal) const;
    void drawMarkers(const Session& session, const Assets& assets) const;
    void drawPickups(const Session& session, const Assets& assets, float time) const;
    void drawWeather(const Palette& pal, float time) const;

    const StageDef* stage_ = nullptr;
    b2Vec2 centre_{0.0f, 0.0f};
    float ppm_ = 45.0f;
    float zoom_ = 1.0f;
    float shakeTime_ = 0.0f;
    float shakeStrength_ = 0.0f;
    b2Vec2 shakeOffset_{0.0f, 0.0f};
    Rng shakeRng_{0x5A4E};
    Noise1D hillNoise_[2]{Noise1D(11), Noise1D(23)};
};

// Mini landscape for stage-select cards (screen space).
void drawStagePreview(const StageDef& stage, Rectangle r, float time);

// Coin / fuel can artwork (world space), shared with the HUD icons.
void drawCoin(b2Vec2 centre, float radius, int value, float spin, const Assets* assets);
void drawFuelCan(b2Vec2 centre, float height, float tilt);

} // namespace hr
