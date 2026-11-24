#include "game/WorldView.hpp"

#include "core/Math.hpp"
#include "core/Rng.hpp"
#include "game/Draw.hpp"

#include <rlgl.h>

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

constexpr float kVisibleMetres = 14.0f;  // vertical metres on screen at rest
const Color kInk{36, 26, 34, 255};

float screenW() { return static_cast<float>(GetScreenWidth()); }
float screenH() { return static_cast<float>(GetScreenHeight()); }

void fillEllipse(b2Vec2 c, float rx, float ry, Color color)
{
    constexpr int kSegments = 28;
    Vector2 pts[kSegments + 2];
    pts[0] = draw::rv(c);
    for (int i = 0; i <= kSegments; ++i) {
        const float a = -kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
        pts[i + 1] = draw::rv({c.x + std::cos(a) * rx, c.y + std::sin(a) * ry});
    }
    DrawTriangleFan(pts, kSegments + 2, color);
}

Color coinColor(int value)
{
    switch (value) {
    case 5: return {232, 150, 70, 255};
    case 25: return {200, 210, 224, 255};
    case 100: return {255, 204, 40, 255};
    default: return {236, 64, 92, 255};
    }
}

float coinRadius(int value)
{
    switch (value) {
    case 5: return 0.36f;
    case 25: return 0.40f;
    case 100: return 0.44f;
    default: return 0.50f;
    }
}

// Silhouettes for the near hill layer and ground props.
void drawTree(Vector2 base, float h, Color trunk, Color leaves)
{
    DrawRectangleV({base.x - h * 0.06f, base.y - h * 0.45f}, {h * 0.12f, h * 0.45f}, trunk);
    DrawCircleV({base.x, base.y - h * 0.62f}, h * 0.30f, leaves);
    DrawCircleV({base.x - h * 0.18f, base.y - h * 0.48f}, h * 0.22f, leaves);
    DrawCircleV({base.x + h * 0.18f, base.y - h * 0.50f}, h * 0.22f, leaves);
}

void drawPine(Vector2 base, float h, Color c, Color snow)
{
    DrawRectangleV({base.x - h * 0.05f, base.y - h * 0.15f}, {h * 0.10f, h * 0.15f}, draw::shade(c, 0.6f));
    for (int i = 0; i < 3; ++i) {
        const float y = base.y - h * (0.12f + 0.25f * static_cast<float>(i));
        const float w = h * (0.34f - 0.08f * static_cast<float>(i));
        DrawTriangle({base.x, y - h * 0.38f}, {base.x - w, y}, {base.x + w, y}, c);
        if (snow.a > 0) DrawTriangle({base.x, y - h * 0.38f}, {base.x - w * 0.35f, y - h * 0.25f}, {base.x + w * 0.35f, y - h * 0.25f}, snow);
    }
}

void drawCactus(Vector2 base, float h, Color c)
{
    const float w = h * 0.16f;
    DrawRectangleRounded({base.x - w * 0.5f, base.y - h, w, h}, 1.0f, 6, c);
    DrawRectangleRounded({base.x - w * 1.9f, base.y - h * 0.62f, w * 0.8f, h * 0.34f}, 1.0f, 6, c);
    DrawRectangleRounded({base.x - w * 1.9f, base.y - h * 0.36f, w * 1.6f, w * 0.7f}, 1.0f, 6, c);
    DrawRectangleRounded({base.x + w * 1.1f, base.y - h * 0.78f, w * 0.8f, h * 0.30f}, 1.0f, 6, c);
    DrawRectangleRounded({base.x + w * 0.3f, base.y - h * 0.55f, w * 1.6f, w * 0.7f}, 1.0f, 6, c);
}

void drawRock(Vector2 base, float h, Color c)
{
    DrawCircleV({base.x, base.y - h * 0.15f}, h * 0.5f, c);
    DrawCircleV({base.x + h * 0.35f, base.y - h * 0.05f}, h * 0.32f, draw::shade(c, 0.9f));
    DrawCircleV({base.x - h * 0.12f, base.y - h * 0.32f}, h * 0.16f, draw::shade(c, 1.15f));
}

void drawSpire(Vector2 base, float h, Color c)
{
    DrawTriangle({base.x, base.y - h}, {base.x - h * 0.22f, base.y}, {base.x + h * 0.22f, base.y}, c);
    DrawTriangle({base.x + h * 0.15f, base.y - h * 0.65f}, {base.x - h * 0.05f, base.y}, {base.x + h * 0.40f, base.y}, draw::shade(c, 0.9f));
}

} // namespace

// ---------------------------------------------------------------------------

void WorldView::reset(const StageDef& stage, b2Vec2 focus)
{
    stage_ = &stage;
    centre_ = {focus.x + 3.0f, focus.y + 1.5f};
    zoom_ = 1.0f;
    ppm_ = screenH() / kVisibleMetres;
    shakeTime_ = 0.0f;
    hillNoise_[0] = Noise1D(stage.seed * 7 + 1);
    hillNoise_[1] = Noise1D(stage.seed * 13 + 5);
}

void WorldView::update(float dt, b2Vec2 focus, b2Vec2 velocity)
{
    const float speed = b2Length(velocity);
    const float lookAhead = clampf(velocity.x * 0.38f, -3.0f, 7.5f);
    const b2Vec2 target{focus.x + 2.5f + lookAhead, focus.y + 1.6f + clampf(velocity.y * 0.12f, -1.5f, 1.5f)};
    centre_.x = approach(centre_.x, target.x, 4.5f, dt);
    centre_.y = approach(centre_.y, target.y, 3.5f, dt);
    // Never let the car leave the screen, even when the smoothing lags behind.
    const float halfW = screenW() * 0.5f / ppm_;
    const float halfH = screenH() * 0.5f / ppm_;
    centre_.x = clampf(centre_.x, focus.x - halfW * 0.55f, focus.x + halfW * 0.75f);
    centre_.y = clampf(centre_.y, focus.y - halfH * 0.45f, focus.y + halfH * 0.6f);

    const float targetZoom = lerpf(1.0f, 0.74f, smoothstep(6.0f, 26.0f, speed));
    zoom_ = approach(zoom_, targetZoom, 1.4f, dt);
    ppm_ = screenH() / kVisibleMetres * zoom_;

    if (shakeTime_ > 0.0f) {
        shakeTime_ -= dt;
        const float s = shakeStrength_ * clampf(shakeTime_ * 3.0f, 0.0f, 1.0f);
        shakeOffset_ = {shakeRng_.range(-s, s), shakeRng_.range(-s, s)};
    } else {
        shakeOffset_ = {0.0f, 0.0f};
    }
}

void WorldView::shake(float strength, float seconds)
{
    shakeStrength_ = std::fmax(shakeStrength_ * (shakeTime_ > 0.0f ? 1.0f : 0.0f), strength);
    shakeTime_ = std::fmax(shakeTime_, seconds);
}

Camera2D WorldView::camera() const
{
    Camera2D cam{};
    const b2Vec2 c = b2Add(centre_, shakeOffset_);
    cam.target = draw::rv(c);
    cam.offset = {screenW() * 0.5f, screenH() * 0.5f};
    cam.zoom = ppm_;
    return cam;
}

Vector2 WorldView::toScreen(b2Vec2 world) const
{
    return GetWorldToScreen2D(draw::rv(world), camera());
}

void WorldView::draw(const Session& session, const VehiclePose& pose, const Particles& particles, const Assets& assets,
                     float time, bool debugPhysics, const std::vector<OtherCar>& others) const
{
    const Palette& pal = paletteFor(session.stage().id);
    rlDisableBackfaceCulling();
    drawSky(pal, time);
    drawHills(pal, 0);
    drawHills(pal, 1);

    BeginMode2D(camera());
    drawTerrain(session, pal, assets);
    drawProps(session, pal);
    drawMarkers(session, assets);
    drawPickups(session, assets, time);
    particles.draw();
    for (const OtherCar& car : others) {
        if (car.def) drawVehicle(*car.def, car.pose, time);
    }
    drawVehicle(session.vehicle().def(), pose, time);
    particles.drawPopups(assets.font(40.0f));

    if (debugPhysics) {
        const Vehicle& v = session.vehicle();
        std::vector<Vehicle::Probe> probes;
        v.probes(probes);
        for (const auto& p : probes) DrawCircleLinesV(draw::rv(p.centre), p.radius, MAGENTA);
        const float x0 = centre_.x - screenW() * 0.5f / ppm_;
        const float x1 = centre_.x + screenW() * 0.5f / ppm_;
        for (float x = x0; x < x1; x += Terrain::kStep) {
            const Terrain& t = session.terrain();
            DrawLineEx(draw::rv(x, t.surfaceHeight(x)), draw::rv(x + Terrain::kStep, t.surfaceHeight(x + Terrain::kStep)),
                       0.04f, t.generator().isIce(x) ? SKYBLUE : RED);
        }
    }
    EndMode2D();
    drawWeather(pal, time);
}

void WorldView::drawSky(const Palette& pal, float time) const
{
    const float w = screenW();
    const float h = screenH();
    DrawRectangleGradientV(0, 0, static_cast<int>(w) + 1, static_cast<int>(h) + 1, pal.skyTop, pal.skyBottom);

    if (pal.stars) {
        Rng rng(77);
        for (int i = 0; i < 140; ++i) {
            const float x = std::fmod(rng.range(0.0f, w * 2.0f) - centre_.x * 2.0f, w * 2.0f);
            const float sx = x < 0.0f ? x + w * 2.0f : x;
            const float sy = rng.range(0.0f, h * 0.75f);
            const float twinkle = 0.6f + 0.4f * std::sin(time * rng.range(1.0f, 4.0f) + static_cast<float>(i));
            DrawCircleV({sx - w * 0.5f, sy}, rng.range(0.8f, 2.0f) * (h / 720.0f), draw::withAlpha(WHITE, twinkle));
        }
    }

    // Sun (or a distant planet on the moon).
    const Vector2 sun{w * 0.80f, h * 0.18f};
    const float r = h * 0.07f;
    if (pal.planet) {
        DrawCircleV(sun, r * 1.6f, {60, 120, 220, 255});
        DrawCircleV({sun.x - r * 0.4f, sun.y - r * 0.2f}, r * 0.7f, {80, 170, 90, 255});
        DrawCircleV({sun.x + r * 0.6f, sun.y + r * 0.5f}, r * 0.45f, {80, 170, 90, 255});
        DrawCircleV({sun.x + r * 0.3f, sun.y - r * 0.9f}, r * 0.5f, draw::withAlpha(WHITE, 0.8f));
        DrawRing(sun, r * 1.6f, r * 1.8f, 0.0f, 360.0f, 48, draw::withAlpha({140, 190, 255, 255}, 0.35f));
    } else {
        for (int i = 3; i >= 1; --i)
            DrawCircleV(sun, r * (1.0f + 0.45f * static_cast<float>(i)), draw::withAlpha(pal.sun, 0.12f));
        DrawCircleV(sun, r, pal.sun);
    }

    // Clouds drift slowly with parallax.
    if (pal.cloud.a > 0) {
        Rng rng(stage_ ? stage_->seed : 1);
        for (int i = 0; i < 7; ++i) {
            const float span = w + h * 0.8f;
            const float base = rng.range(0.0f, span);
            float x = std::fmod(base - centre_.x * ppm_ * 0.06f - time * 6.0f * (h / 720.0f), span);
            if (x < 0.0f) x += span;
            x -= h * 0.4f;
            const float y = rng.range(h * 0.06f, h * 0.38f);
            const float s = rng.range(0.6f, 1.2f) * h / 720.0f;
            const Color c = pal.cloud;
            DrawCircleV({x, y}, 34.0f * s, c);
            DrawCircleV({x + 36.0f * s, y - 14.0f * s}, 40.0f * s, c);
            DrawCircleV({x + 78.0f * s, y}, 32.0f * s, c);
            DrawRectangleRounded({x - 10.0f * s, y - 6.0f * s, 110.0f * s, 38.0f * s}, 1.0f, 8, c);
        }
    }
}

void WorldView::drawHills(const Palette& pal, int layer) const
{
    const float w = screenW();
    const float h = screenH();
    const float parallax = layer == 0 ? 0.12f : 0.32f;
    const float scale = layer == 0 ? 0.5f : 0.75f;  // apparent size of the layer
    const float layerPpm = ppm_ * scale;
    const float horizon = h * (layer == 0 ? 0.58f : 0.70f) + (centre_.y * ppm_ * parallax * 0.25f);
    const float amplitude = (layer == 0 ? 7.5f : 5.0f);
    const float wavelength = layer == 0 ? 60.0f : 32.0f;
    const Noise1D& noise = hillNoise_[layer];
    const Color color = layer == 0 ? pal.farHills : pal.nearHills;

    auto profile = [&](float sx) {
        const float wx = centre_.x * parallax + (sx - w * 0.5f) / layerPpm;
        return horizon - (noise.fbm(wx / wavelength, 3) * amplitude + amplitude * 0.4f) * layerPpm;
    };

    constexpr float kStepPx = 8.0f;
    std::vector<Vector2> strip;
    strip.reserve(static_cast<size_t>(w / kStepPx) * 2 + 8);
    for (float sx = -kStepPx; sx <= w + kStepPx; sx += kStepPx) {
        strip.push_back({sx, profile(sx)});
        strip.push_back({sx, h + 2.0f});
    }
    DrawTriangleStrip(strip.data(), static_cast<int>(strip.size()), color);

    // Scenery silhouettes on the near layer.
    if (layer == 1) {
        const float spacing = 7.0f;  // layer metres between candidates
        const float wx0 = centre_.x * parallax - (w * 0.5f) / layerPpm - spacing;
        const float wx1 = centre_.x * parallax + (w * 0.5f) / layerPpm + spacing;
        const Color dark = draw::shade(color, 0.82f);
        for (float slot = std::floor(wx0 / spacing); slot * spacing < wx1; slot += 1.0f) {
            Rng rng(hashCombine(stage_ ? stage_->seed : 3, static_cast<uint64_t>(static_cast<int64_t>(slot))));
            if (!rng.chance(0.45f)) continue;
            const float wx = slot * spacing + rng.range(0.0f, spacing);
            const float sx = (wx - centre_.x * parallax) * layerPpm + w * 0.5f;
            const Vector2 base{sx, profile(sx) + 3.0f};
            const float size = rng.range(2.0f, 3.6f) * layerPpm;
            switch (pal.scenery) {
            case Scenery::Trees: drawTree(base, size, draw::shade(color, 0.7f), dark); break;
            case Scenery::Cacti: drawCactus(base, size * 0.8f, dark); break;
            case Scenery::Pines: drawPine(base, size * 1.2f, dark, draw::withAlpha(WHITE, 0.7f)); break;
            case Scenery::Spires: drawPine(base, size * 1.1f, dark, BLANK); break;
            case Scenery::Rocks: drawSpire(base, size, dark); break;
            case Scenery::Boulders: drawRock(base, size * 0.6f, dark); break;
            }
        }
    }
}

void WorldView::drawTerrain(const Session& session, const Palette& pal, const Assets& assets) const
{
    const Terrain& terrain = session.terrain();
    const TerrainGenerator& gen = terrain.generator();
    const float halfW = screenW() * 0.5f / ppm_ + 2.0f;
    const float halfH = screenH() * 0.5f / ppm_ + 2.0f;
    const int i0 = static_cast<int>(std::floor((centre_.x - halfW) / Terrain::kStep));
    const int i1 = static_cast<int>(std::ceil((centre_.x + halfW) / Terrain::kStep));
    const float bottom = centre_.y - halfH - 1.0f;

    // Sample the visible polyline once.
    const int n = i1 - i0 + 1;
    std::vector<b2Vec2> top(static_cast<size_t>(n));
    std::vector<b2Vec2> normal(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k) top[static_cast<size_t>(k)] = {Terrain::sampleX(i0 + k), terrain.sampleHeight(i0 + k)};
    for (int k = 0; k < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(std::max(0, k - 1))];
        const b2Vec2 b = top[static_cast<size_t>(std::min(n - 1, k + 1))];
        const b2Vec2 tangent = b2Normalize(b2Sub(b, a));
        normal[static_cast<size_t>(k)] = {-tangent.y, tangent.x};
    }

    // Ground body: textured, darkening with depth.
    const float tile = 3.2f;
    const Color deep = draw::shade(pal.ground, 0.72f);
    rlSetTexture(assets.groundTexture.id);
    rlBegin(RL_QUADS);
    for (int k = 0; k + 1 < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(k)];
        const b2Vec2 b = top[static_cast<size_t>(k + 1)];
        const float mid = 7.0f;
        // Band 1: surface down to 7 m deep, gradient.
        rlColor4ub(pal.ground.r, pal.ground.g, pal.ground.b, 255);
        rlTexCoord2f(a.x / tile, -a.y / tile);
        rlVertex2f(a.x, -a.y);
        rlColor4ub(deep.r, deep.g, deep.b, 255);
        rlTexCoord2f(a.x / tile, -(a.y - mid) / tile);
        rlVertex2f(a.x, -(a.y - mid));
        rlTexCoord2f(b.x / tile, -(b.y - mid) / tile);
        rlVertex2f(b.x, -(b.y - mid));
        rlColor4ub(pal.ground.r, pal.ground.g, pal.ground.b, 255);
        rlTexCoord2f(b.x / tile, -b.y / tile);
        rlVertex2f(b.x, -b.y);
        // Band 2: solid to the bottom of the screen.
        if (std::fmin(a.y, b.y) - mid > bottom) {
            rlColor4ub(deep.r, deep.g, deep.b, 255);
            rlTexCoord2f(a.x / tile, -(a.y - mid) / tile);
            rlVertex2f(a.x, -(a.y - mid));
            rlTexCoord2f(a.x / tile, -bottom / tile);
            rlVertex2f(a.x, -bottom);
            rlTexCoord2f(b.x / tile, -bottom / tile);
            rlVertex2f(b.x, -bottom);
            rlTexCoord2f(b.x / tile, -(b.y - mid) / tile);
            rlVertex2f(b.x, -(b.y - mid));
        }
    }
    rlEnd();

    // Surface strip (grass / sand / snow), following the normals.
    const float thick = 0.48f;
    rlSetTexture(assets.surfaceTexture.id);
    rlBegin(RL_QUADS);
    for (int k = 0; k + 1 < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(k)];
        const b2Vec2 b = top[static_cast<size_t>(k + 1)];
        const b2Vec2 na = normal[static_cast<size_t>(k)];
        const b2Vec2 nb = normal[static_cast<size_t>(k + 1)];
        const bool ice = gen.isIce((a.x + b.x) * 0.5f);
        const Color c = ice ? pal.ice : pal.surface;
        const b2Vec2 a2 = b2MulAdd(a, -thick, na);
        const b2Vec2 b2 = b2MulAdd(b, -thick, nb);
        rlColor4ub(c.r, c.g, c.b, 255);
        rlTexCoord2f(a.x / 2.5f, 0.0f);
        rlVertex2f(a.x, -a.y);
        rlTexCoord2f(a.x / 2.5f, 1.0f);
        rlVertex2f(a2.x, -a2.y);
        rlTexCoord2f(b.x / 2.5f, 1.0f);
        rlVertex2f(b2.x, -b2.y);
        rlTexCoord2f(b.x / 2.5f, 0.0f);
        rlVertex2f(b.x, -b.y);
    }
    rlEnd();
    rlSetTexture(0);

    // Darker underside of the strip and a crisp top edge.
    for (int k = 0; k + 1 < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(k)];
        const b2Vec2 b = top[static_cast<size_t>(k + 1)];
        const b2Vec2 na = normal[static_cast<size_t>(k)];
        const b2Vec2 nb = normal[static_cast<size_t>(k + 1)];
        const bool ice = gen.isIce((a.x + b.x) * 0.5f);
        const Color edge = ice ? draw::shade(pal.ice, 0.8f) : pal.surfaceEdge;
        const Vector2 q[4] = {draw::rv(b2MulAdd(a, -thick * 0.78f, na)), draw::rv(b2MulAdd(a, -thick, na)),
                              draw::rv(b2MulAdd(b, -thick, nb)), draw::rv(b2MulAdd(b, -thick * 0.78f, nb))};
        DrawTriangle(q[0], q[1], q[2], edge);
        DrawTriangle(q[0], q[2], q[3], edge);
        DrawLineEx(draw::rv(a), draw::rv(b), 0.06f, draw::withAlpha(kInk, 0.55f));
        if (ice && k % 6 == 0) {
            DrawLineEx(draw::rv(b2MulAdd(a, -0.12f, na)), draw::rv(b2MulAdd(b, -0.12f, nb)), 0.05f, draw::withAlpha(WHITE, 0.8f));
        }
    }
}

void WorldView::drawProps(const Session& session, const Palette& pal) const
{
    const Terrain& terrain = session.terrain();
    const float halfW = screenW() * 0.5f / ppm_ + 2.0f;
    const float spacing = 3.0f;
    for (float slot = std::floor((centre_.x - halfW) / spacing); slot * spacing < centre_.x + halfW; slot += 1.0f) {
        if (slot * spacing < 6.0f && slot * spacing > -20.0f) continue;  // keep the start line clean
        Rng rng(hashCombine(session.stage().seed ^ 0x9909ull, static_cast<uint64_t>(static_cast<int64_t>(slot))));
        if (!rng.chance(0.55f)) continue;
        const float x = slot * spacing + rng.range(0.0f, spacing);
        if (terrain.generator().isIce(x)) continue;
        const b2Vec2 base{x, terrain.surfaceHeight(x) - 0.05f};
        const Vector2 at = draw::rv(base);
        const float roll = rng.unit();
        switch (pal.scenery) {
        case Scenery::Trees:
        case Scenery::Spires:
            if (roll < 0.55f) {
                // Grass tuft.
                const Color g = draw::shade(pal.surface, 0.85f);
                for (int i = -1; i <= 1; ++i)
                    DrawTriangle({at.x + 0.12f * static_cast<float>(i) - 0.06f, at.y}, {at.x + 0.12f * static_cast<float>(i) + 0.04f * static_cast<float>(i), at.y - 0.35f},
                                 {at.x + 0.12f * static_cast<float>(i) + 0.06f, at.y}, g);
            } else if (roll < 0.8f) {
                const Color petal = rng.chance(0.5f) ? Color{250, 240, 120, 255} : Color{250, 140, 170, 255};
                DrawLineEx(at, {at.x, at.y - 0.32f}, 0.04f, draw::shade(pal.surface, 0.7f));
                DrawCircleV({at.x, at.y - 0.34f}, 0.09f, petal);
                DrawCircleV({at.x, at.y - 0.34f}, 0.04f, {250, 200, 60, 255});
            } else {
                drawRock(at, 0.4f, {150, 150, 140, 255});
            }
            break;
        case Scenery::Cacti:
            if (roll < 0.12f) drawCactus(at, 1.3f, {80, 150, 70, 255});
            else if (roll < 0.5f) drawRock(at, 0.35f, {190, 140, 90, 255});
            break;
        case Scenery::Pines:
            if (roll < 0.15f) drawPine(at, 1.6f, {50, 110, 80, 255}, WHITE);
            else if (roll < 0.5f) DrawCircleV({at.x, at.y + 0.05f}, 0.32f, {250, 252, 255, 255});
            break;
        case Scenery::Rocks:
        case Scenery::Boulders:
            if (roll < 0.5f) drawRock(at, rng.range(0.3f, 0.6f), draw::shade(pal.surfaceEdge, rng.range(0.8f, 1.1f)));
            break;
        }
    }
}

void WorldView::drawMarkers(const Session& session, const Assets& assets) const
{
    const Terrain& terrain = session.terrain();
    const float halfW = screenW() * 0.5f / ppm_ + 3.0f;
    const Font& font = assets.font(40.0f);
    char label[32];

    // Distance signs every 100 m.
    for (float x = std::ceil((centre_.x - halfW) / 100.0f) * 100.0f; x < centre_.x + halfW; x += 100.0f) {
        if (x <= 0.0f) continue;
        const float y = terrain.surfaceHeight(x);
        const Vector2 base = draw::rv(x, y);
        DrawRectangleV({base.x - 0.06f, base.y - 1.9f}, {0.12f, 1.9f}, {110, 76, 46, 255});
        const Rectangle board{base.x - 0.75f, base.y - 2.45f, 1.5f, 0.62f};
        DrawRectangleRounded({board.x - 0.05f, board.y - 0.05f, board.width + 0.1f, board.height + 0.1f}, 0.3f, 6, kInk);
        DrawRectangleRounded(board, 0.3f, 6, {214, 168, 98, 255});
        std::snprintf(label, sizeof(label), "%dm", static_cast<int>(x));
        const Vector2 dim = MeasureTextEx(font, label, 0.42f, 0.01f);
        DrawTextEx(font, label, {base.x - dim.x * 0.5f, board.y + (board.height - dim.y) * 0.5f}, 0.42f, 0.01f, kInk);
    }

    // Start line.
    if (centre_.x - halfW < 2.0f) {
        const Vector2 base = draw::rv(-1.2f, 0.0f);
        for (int i = 0; i < 8; ++i) {
            DrawRectangleV({base.x, base.y - 2.6f + 0.3f * static_cast<float>(i)}, {0.15f, 0.3f}, i % 2 ? WHITE : kInk);
            DrawRectangleV({base.x + 0.15f, base.y - 2.6f + 0.3f * static_cast<float>(i)}, {0.15f, 0.3f}, i % 2 ? kInk : WHITE);
        }
    }

    // Best-distance flag.
    const float best = session.bestDistance();
    if (best > 20.0f && std::fabs(best - centre_.x) < halfW) {
        const float y = terrain.surfaceHeight(best);
        const Vector2 base = draw::rv(best, y);
        DrawRectangleV({base.x - 0.06f, base.y - 3.4f}, {0.12f, 3.4f}, {230, 230, 236, 255});
        const float wave = std::sin(static_cast<float>(GetTime()) * 4.0f) * 0.08f;
        const Vector2 f0{base.x + 0.06f, base.y - 3.35f};
        DrawTriangle(f0, {f0.x, f0.y + 0.9f}, {f0.x + 1.4f, f0.y + 0.45f + wave}, {226, 52, 60, 255});
        const char* text = "BEST";
        const Vector2 dim = MeasureTextEx(font, text, 0.36f, 0.01f);
        DrawTextEx(font, text, {base.x - dim.x * 0.5f, base.y - 3.95f}, 0.36f, 0.01f, kInk);
    }
}

void drawCoin(b2Vec2 c, float r, int value, float spin, const Assets* assets)
{
    const float w = std::fmax(0.12f, std::fabs(spin));
    const Color base = coinColor(value);
    fillEllipse(c, r * w + 0.04f, r + 0.04f, kInk);
    fillEllipse(c, r * w, r, draw::shade(base, 0.78f));
    fillEllipse({c.x + 0.02f * w, c.y + 0.01f}, r * w * 0.78f, r * 0.78f, base);
    fillEllipse({c.x - r * w * 0.25f, c.y + r * 0.3f}, r * w * 0.22f, r * 0.14f, draw::withAlpha(WHITE, 0.6f));
    if (assets && w > 0.75f) {
        char label[8];
        std::snprintf(label, sizeof(label), "%d", value);
        const float size = value >= 100 ? r * 0.75f : r * 0.95f;
        const Font& font = assets->font(40.0f);
        const Vector2 dim = MeasureTextEx(font, label, size, 0.0f);
        const Vector2 at = draw::rv(c);
        DrawTextEx(font, label, {at.x - dim.x * 0.5f, at.y - dim.y * 0.5f}, size, 0.0f, draw::withAlpha(draw::shade(base, 0.55f), (w - 0.75f) * 4.0f));
    }
}

void drawFuelCan(b2Vec2 c, float h, float tilt)
{
    const float w = h * 0.78f;
    const b2Transform t{c, b2MakeRot(tilt)};
    const draw::Shape body(std::vector<b2Vec2>{{-w * 0.5f, -h * 0.5f}, {w * 0.5f, -h * 0.5f}, {w * 0.5f, h * 0.30f},
                                               {w * 0.22f, h * 0.5f}, {-w * 0.5f, h * 0.5f}});
    draw::fillOutlined(body, t, {226, 46, 46, 255}, kInk, 0.05f);
    draw::line(b2TransformPoint(t, {-w * 0.32f, h * 0.5f}), b2TransformPoint(t, {w * 0.05f, h * 0.5f}), 0.09f, kInk);
    draw::line(b2TransformPoint(t, {w * 0.3f, h * 0.38f}), b2TransformPoint(t, {w * 0.48f, h * 0.58f}), 0.11f, kInk);
    // Emblem: a white drop.
    const b2Vec2 e = b2TransformPoint(t, {0.0f, -h * 0.06f});
    draw::circle(e, w * 0.2f, WHITE);
    const Vector2 tip = draw::rv(b2TransformPoint(t, {0.0f, h * 0.22f}));
    const Vector2 l = draw::rv(b2TransformPoint(t, {-w * 0.18f, -h * 0.02f}));
    const Vector2 r = draw::rv(b2TransformPoint(t, {w * 0.18f, -h * 0.02f}));
    DrawTriangle(tip, l, r, WHITE);
    // Shine.
    draw::line(b2TransformPoint(t, {-w * 0.36f, -h * 0.34f}), b2TransformPoint(t, {-w * 0.36f, h * 0.30f}), 0.06f,
               draw::withAlpha(WHITE, 0.35f));
}

void WorldView::drawPickups(const Session& session, const Assets& assets, float time) const
{
    const float halfW = screenW() * 0.5f / ppm_ + 2.0f;
    session.pickups().forEachVisible(centre_.x - halfW, centre_.x + halfW, [&](const Pickup& p) {
        if (p.kind == PickupKind::Coin) {
            const float spin = std::cos(time * 2.6f + p.pos.x * 0.35f);
            drawCoin(p.pos, coinRadius(p.value), p.value, spin, &assets);
        } else {
            const float bob = std::sin(time * 2.5f + p.pos.x) * 0.08f;
            const b2Vec2 c{p.pos.x, p.pos.y + bob};
            const float pulse = 0.5f + 0.5f * std::sin(time * 4.0f);
            DrawCircleV(draw::rv(c), 0.85f + 0.1f * pulse, draw::withAlpha({255, 240, 160, 255}, 0.25f + 0.15f * pulse));
            drawFuelCan(c, 0.82f, std::sin(time * 1.8f) * 0.08f);
        }
    });
}

void WorldView::drawWeather(const Palette& pal, float time) const
{
    if (!pal.snowfall) return;
    const float w = screenW();
    const float h = screenH();
    Rng rng(99);
    for (int i = 0; i < 90; ++i) {
        const float speed = rng.range(30.0f, 70.0f) * h / 720.0f;
        const float x0 = rng.range(0.0f, w);
        const float y0 = rng.range(0.0f, h);
        float x = std::fmod(x0 - centre_.x * ppm_ * 0.6f + std::sin(time + static_cast<float>(i)) * 20.0f, w);
        if (x < 0.0f) x += w;
        float y = std::fmod(y0 + time * speed + centre_.y * ppm_ * 0.6f, h);
        if (y < 0.0f) y += h;
        DrawCircleV({x, y}, rng.range(1.5f, 3.5f) * h / 720.0f, draw::withAlpha(WHITE, 0.85f));
    }
}

void drawStagePreview(const StageDef& stage, Rectangle r, float time)
{
    const Palette& pal = paletteFor(stage.id);
    BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
    DrawRectangleGradientV(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width) + 1,
                           static_cast<int>(r.height) + 1, pal.skyTop, pal.skyBottom);
    if (pal.stars) {
        Rng rng(5);
        for (int i = 0; i < 30; ++i) DrawCircleV({r.x + rng.range(0.0f, r.width), r.y + rng.range(0.0f, r.height * 0.6f)}, 1.2f, WHITE);
    }
    if (pal.planet) DrawCircleV({r.x + r.width * 0.78f, r.y + r.height * 0.22f}, r.height * 0.11f, {60, 120, 220, 255});
    else DrawCircleV({r.x + r.width * 0.78f, r.y + r.height * 0.22f}, r.height * 0.09f, pal.sun);

    TerrainGenerator gen(stage);
    Noise1D far(stage.seed * 7 + 1);
    std::vector<Vector2> strip;
    for (int layer = 0; layer < 3; ++layer) {
        strip.clear();
        for (float sx = 0.0f; sx <= r.width + 4.0f; sx += 4.0f) {
            float y = 0.0f;
            if (layer == 0) y = r.y + r.height * 0.52f - far.fbm(sx / r.width * 3.0f + time * 0.02f, 3) * r.height * 0.12f;
            else if (layer == 1) y = r.y + r.height * 0.66f - far.fbm(sx / r.width * 5.0f + 9.0f, 3) * r.height * 0.10f;
            else y = r.y + r.height * 0.80f - gen.height(400.0f + sx / r.width * 120.0f) * r.height * 0.018f;
            strip.push_back({r.x + sx, y});
            strip.push_back({r.x + sx, r.y + r.height + 2.0f});
        }
        const Color c = layer == 0 ? pal.farHills : layer == 1 ? pal.nearHills : pal.surface;
        DrawTriangleStrip(strip.data(), static_cast<int>(strip.size()), c);
        if (layer == 2) {
            for (size_t i = 0; i < strip.size(); i += 2) strip[i].y += r.height * 0.035f;
            DrawTriangleStrip(strip.data(), static_cast<int>(strip.size()), pal.ground);
        }
    }
    EndScissorMode();
}

} // namespace hr
