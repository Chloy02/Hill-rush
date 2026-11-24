#include "game/Particles.hpp"

#include "core/Math.hpp"
#include "game/Draw.hpp"

#include <algorithm>
#include <cmath>

namespace hr {

void Particles::clear()
{
    particles_.clear();
    popups_.clear();
}

void Particles::add(const Particle& p)
{
    if (particles_.size() >= kMax) particles_.erase(particles_.begin());
    particles_.push_back(p);
}

void Particles::popup(b2Vec2 pos, const std::string& text, Color color, float size)
{
    popups_.push_back({pos, text, color, 1.1f, 1.1f, size});
}

void Particles::update(float dt)
{
    for (Particle& p : particles_) {
        p.life -= dt;
        p.vel.y -= p.gravity * dt;
        const float damp = std::exp(-p.drag * dt);
        p.vel = b2MulSV(damp, p.vel);
        p.pos = b2MulAdd(p.pos, dt, p.vel);
        p.size = std::fmax(0.0f, p.size + p.growth * dt);
        p.rotation += p.spin * dt;
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.life <= 0.0f; }),
                     particles_.end());

    for (Popup& p : popups_) {
        p.life -= dt;
        p.pos.y += dt * 1.6f;
    }
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(), [](const Popup& p) { return p.life <= 0.0f; }),
                  popups_.end());
}

void Particles::draw() const
{
    for (const Particle& p : particles_) {
        const float t = clampf(p.life / p.maxLife, 0.0f, 1.0f);
        const Color c = p.fade ? draw::withAlpha(p.color, t) : p.color;
        const Vector2 at = draw::rv(p.pos);
        switch (p.shape) {
        case ParticleShape::Circle:
            DrawCircleV(at, p.size, c);
            break;
        case ParticleShape::Square:
            DrawRectanglePro({at.x, at.y, p.size * 2.0f, p.size * 2.0f}, {p.size, p.size}, radToDeg(p.rotation), c);
            break;
        case ParticleShape::Star:
            DrawPoly(at, 4, p.size * 1.3f, radToDeg(p.rotation), c);
            DrawPoly(at, 4, p.size * 0.8f, radToDeg(p.rotation) + 45.0f, c);
            break;
        case ParticleShape::Spark: {
            const b2Vec2 tail = b2MulAdd(p.pos, -0.04f, p.vel);
            DrawLineEx(at, draw::rv(tail), p.size, c);
            break;
        }
        }
    }
}

void Particles::drawPopups(const Font& font) const
{
    for (const Popup& p : popups_) {
        const float t = p.life / p.maxLife;
        const float scale = t > 0.85f ? 1.0f + (t - 0.85f) * 3.0f : 1.0f;
        const float size = p.size * scale;
        const Vector2 dim = MeasureTextEx(font, p.text.c_str(), size, 0.02f);
        Vector2 at = draw::rv(p.pos);
        at.x -= dim.x * 0.5f;
        at.y -= dim.y * 0.5f;
        const float alpha = clampf(t * 2.5f, 0.0f, 1.0f);
        const float o = size * 0.07f;
        const Color outline = draw::withAlpha({30, 26, 38, 255}, alpha);
        for (int i = 0; i < 8; ++i) {
            const float a = kTwoPi * static_cast<float>(i) / 8.0f;
            DrawTextEx(font, p.text.c_str(), {at.x + std::cos(a) * o, at.y + std::sin(a) * o}, size, 0.02f, outline);
        }
        DrawTextEx(font, p.text.c_str(), at, size, 0.02f, draw::withAlpha(p.color, alpha));
    }
}

void Particles::dust(b2Vec2 at, b2Vec2 direction, Color color, float strength)
{
    const int n = 1 + static_cast<int>(strength * 2.0f);
    for (int i = 0; i < n; ++i) {
        Particle p{};
        p.pos = {at.x + rng_.range(-0.15f, 0.15f), at.y + rng_.range(0.0f, 0.1f)};
        const float speed = rng_.range(1.0f, 3.5f) * (0.5f + strength);
        p.vel = {direction.x * speed + rng_.range(-0.6f, 0.6f), std::fabs(direction.y) * speed + rng_.range(0.6f, 2.4f)};
        p.maxLife = p.life = rng_.range(0.35f, 0.8f);
        p.size = rng_.range(0.05f, 0.13f);
        p.growth = 0.12f;
        p.gravity = 7.0f;
        p.drag = 1.2f;
        p.color = draw::shade(color, rng_.range(0.85f, 1.15f));
        p.shape = rng_.chance(0.5f) ? ParticleShape::Circle : ParticleShape::Square;
        p.spin = rng_.range(-8.0f, 8.0f);
        p.fade = true;
        add(p);
    }
}

void Particles::mudSplash(b2Vec2 at, b2Vec2 direction, float strength)
{
    const int n = 2 + static_cast<int>(strength * 3.0f);
    const Color mudBase{82, 54, 32, 240};
    for (int i = 0; i < n; ++i) {
        Particle p{};
        p.pos = {at.x + rng_.range(-0.16f, 0.16f), at.y + rng_.range(0.0f, 0.12f)};
        const float spd = rng_.range(1.8f, 4.8f) * (0.5f + strength);
        p.vel = {direction.x * spd + rng_.range(-0.9f, 0.9f), std::fabs(direction.y) * spd + rng_.range(1.4f, 3.6f)};
        p.maxLife = p.life = rng_.range(0.35f, 0.70f);
        p.size = rng_.range(0.05f, 0.11f);
        p.growth = -0.02f;
        p.gravity = 11.0f;
        p.drag = 0.8f;
        p.color = draw::shade(mudBase, rng_.range(0.75f, 1.15f));
        p.shape = rng_.chance(0.65f) ? ParticleShape::Circle : ParticleShape::Square;
        p.spin = rng_.range(-10.0f, 10.0f);
        p.fade = true;
        add(p);
    }
}

void Particles::exhaust(b2Vec2 at, b2Vec2 direction, float throttle)
{
    Particle p{};
    p.pos = at;
    p.vel = b2MulAdd({rng_.range(-0.2f, 0.2f), rng_.range(0.4f, 0.9f)}, rng_.range(0.6f, 1.4f) * (0.5f + throttle), direction);
    p.maxLife = p.life = rng_.range(0.5f, 0.9f);
    p.size = rng_.range(0.07f, 0.12f);
    p.growth = 0.35f;
    p.gravity = -0.6f;
    p.drag = 1.5f;
    const unsigned char g = static_cast<unsigned char>(rng_.range(150.0f, 200.0f));
    p.color = {g, g, static_cast<unsigned char>(g + 8), 150};
    p.shape = ParticleShape::Circle;
    p.fade = true;
    add(p);
}

void Particles::flame(b2Vec2 at, b2Vec2 direction, Color core, Color outer, float speed)
{
    const float len = b2Length(direction);
    const b2Vec2 dir = len > 1e-4f ? b2MulSV(1.0f / len, direction) : b2Vec2{0.0f, -1.0f};
    const b2Vec2 perp{-dir.y, dir.x};
    const int n = rng_.chance(0.5f) ? 3 : 2;
    for (int i = 0; i < n; ++i) {
        Particle p{};
        p.pos = b2MulAdd(at, rng_.range(-0.04f, 0.04f), perp);
        const float spd = speed * rng_.range(0.75f, 1.15f);
        const float spread = rng_.range(-0.22f, 0.22f) * speed;
        p.vel = b2MulAdd(b2MulSV(spd, dir), spread, perp);
        p.maxLife = p.life = (i == 0) ? rng_.range(0.08f, 0.14f) : rng_.range(0.11f, 0.19f);
        p.size = (i == 0) ? rng_.range(0.08f, 0.13f) : rng_.range(0.11f, 0.18f);
        p.growth = -p.size / p.maxLife * 0.85f;
        p.gravity = -1.5f;
        p.drag = 3.5f;
        p.color = (i == 0) ? core : draw::mix(core, outer, rng_.range(0.35f, 1.0f));
        p.shape = ParticleShape::Circle;
        p.fade = true;
        add(p);
    }
}

void Particles::sparks(b2Vec2 at, int count, float speed)
{
    const Color yellow{255, 245, 130, 255};
    const Color orange{255, 125, 25, 255};
    for (int i = 0; i < count; ++i) {
        Particle p{};
        p.pos = {at.x + rng_.range(-0.12f, 0.12f), at.y + rng_.range(-0.08f, 0.08f)};
        const float a = rng_.range(-0.4f, kPi + 0.4f);
        const float spd = speed * rng_.range(0.45f, 1.15f);
        p.vel = {std::cos(a) * spd, std::sin(a) * spd + rng_.range(0.4f, 1.8f)};
        p.maxLife = p.life = rng_.range(0.24f, 0.52f);
        p.size = rng_.range(0.035f, 0.065f);
        p.growth = -0.04f;
        p.gravity = 12.0f;
        p.drag = 1.4f;
        p.color = draw::mix(yellow, orange, rng_.range(0.0f, 1.0f));
        p.shape = ParticleShape::Spark;
        p.fade = true;
        add(p);
    }
}

void Particles::sparkle(b2Vec2 at, Color color, int count)
{
    for (int i = 0; i < count; ++i) {
        Particle p{};
        p.pos = at;
        const float a = rng_.range(0.0f, kTwoPi);
        const float s = rng_.range(1.5f, 4.5f);
        p.vel = {std::cos(a) * s, std::sin(a) * s};
        p.maxLife = p.life = rng_.range(0.3f, 0.6f);
        p.size = rng_.range(0.06f, 0.12f);
        p.growth = -0.1f;
        p.gravity = 2.0f;
        p.drag = 3.0f;
        p.color = color;
        p.shape = ParticleShape::Star;
        p.spin = rng_.range(-6.0f, 6.0f);
        p.fade = true;
        add(p);
    }
}

void Particles::burst(b2Vec2 at, Color color, int count, float speed)
{
    for (int i = 0; i < count; ++i) {
        Particle p{};
        p.pos = at;
        const float a = rng_.range(0.15f, kPi - 0.15f);
        const float s = rng_.range(0.4f, 1.0f) * speed;
        p.vel = {std::cos(a) * s, std::sin(a) * s};
        p.maxLife = p.life = rng_.range(0.4f, 0.9f);
        p.size = rng_.range(0.06f, 0.16f);
        p.growth = 0.1f;
        p.gravity = 8.0f;
        p.drag = 1.0f;
        p.color = draw::shade(color, rng_.range(0.85f, 1.15f));
        p.shape = ParticleShape::Circle;
        p.fade = true;
        add(p);
    }
}

void Particles::dizzyStars(b2Vec2 at)
{
    for (int i = 0; i < 6; ++i) {
        Particle p{};
        const float a = kTwoPi * static_cast<float>(i) / 6.0f;
        p.pos = {at.x + std::cos(a) * 0.5f, at.y + 0.4f + std::sin(a) * 0.2f};
        p.vel = {-std::sin(a) * 1.5f, std::cos(a) * 0.6f + 0.8f};
        p.maxLife = p.life = 1.6f;
        p.size = 0.12f;
        p.gravity = 0.0f;
        p.drag = 0.5f;
        p.color = {255, 230, 80, 255};
        p.shape = ParticleShape::Star;
        p.spin = 4.0f;
        p.fade = true;
        add(p);
    }
}

} // namespace hr
