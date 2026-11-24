#pragma once

#include "core/Rng.hpp"

#include <box2d/math_functions.h>
#include <raylib.h>

#include <string>
#include <vector>

namespace hr {

enum class ParticleShape { Circle, Square, Star, Spark };

struct Particle {
    b2Vec2 pos;
    b2Vec2 vel;
    float life;
    float maxLife;
    float size;
    float growth;      // size change per second
    float rotation;
    float spin;
    float gravity;     // m/s^2 downwards (negative floats up)
    float drag;        // velocity damping per second
    Color color;
    ParticleShape shape;
    bool fade;
};

// Floating world-space text such as "+25" or "FUEL".
struct Popup {
    b2Vec2 pos;
    std::string text;
    Color color;
    float life;
    float maxLife;
    float size;  // metres
};

class Particles {
public:
    static constexpr size_t kMax = 1500;

    void clear();
    void update(float dt);
    void draw() const;           // world space, inside BeginMode2D
    void drawPopups(const Font& font) const;

    void add(const Particle& p);
    void popup(b2Vec2 pos, const std::string& text, Color color, float size = 0.55f);

    // Effects ---------------------------------------------------------------
    void dust(b2Vec2 at, b2Vec2 direction, Color color, float strength);
    void exhaust(b2Vec2 at, b2Vec2 direction, float throttle);
    void sparkle(b2Vec2 at, Color color, int count);
    void burst(b2Vec2 at, Color color, int count, float speed);
    void dizzyStars(b2Vec2 at);

    Rng& rng() { return rng_; }
    size_t count() const { return particles_.size(); }

private:
    std::vector<Particle> particles_;
    std::vector<Popup> popups_;
    Rng rng_{0xD057};
};

} // namespace hr
