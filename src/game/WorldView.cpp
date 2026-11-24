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

void drawStalagmite(Vector2 base, float h, Color c, Color crystalGlow, bool withCrystal)
{
    DrawTriangle({base.x, base.y - h}, {base.x - h * 0.24f, base.y}, {base.x + h * 0.24f, base.y}, c);
    DrawTriangle({base.x + h * 0.18f, base.y - h * 0.62f}, {base.x, base.y}, {base.x + h * 0.38f, base.y}, draw::shade(c, 0.82f));
    if (withCrystal) {
        const float ch = h * 0.44f;
        const Vector2 cBase{base.x - h * 0.16f, base.y};
        DrawCircleV({cBase.x, cBase.y - ch * 0.5f}, ch * 0.65f, draw::withAlpha(crystalGlow, 0.18f));
        DrawTriangle({cBase.x - ch * 0.16f, cBase.y - ch}, {cBase.x - ch * 0.26f, cBase.y}, {cBase.x + ch * 0.04f, cBase.y}, crystalGlow);
        DrawTriangle({cBase.x + ch * 0.14f, cBase.y - ch * 0.72f}, {cBase.x - ch * 0.02f, cBase.y}, {cBase.x + ch * 0.26f, cBase.y},
                     draw::shade(crystalGlow, 1.15f));
    }
}

void drawBridges(const Session& session, const Palette& pal)
{
    (void)pal;
    const Color timberLight{168, 112, 64, 255};
    const Color timberDark{146, 94, 52, 255};
    const Color postWood{116, 74, 42, 255};
    const Color ropeColor{174, 146, 98, 225};

    for (const auto& [chunk, list] : session.terrain().bridges()) {
        (void)chunk;
        for (const Terrain::Bridge& b : list) {
            const float postH = 1.15f;
            const float postDepth = 0.35f;
            const b2Vec2 topL{b.x0, b.y0 + postH};
            const b2Vec2 topR{b.x1, b.y1 + postH};

            // Sagging suspension rope across the bridge posts, with vertical hangers.
            constexpr int kRopeSegs = 12;
            const float sag = 0.52f;
            auto ropeAt = [&](float u) -> b2Vec2 {
                b2Vec2 pt = b2Lerp(topL, topR, u);
                pt.y -= sag * 4.0f * u * (1.0f - u);
                return pt;
            };
            for (int s = 0; s < kRopeSegs; ++s) {
                const float u0 = static_cast<float>(s) / static_cast<float>(kRopeSegs);
                const float u1 = static_cast<float>(s + 1) / static_cast<float>(kRopeSegs);
                const b2Vec2 r0 = ropeAt(u0);
                const b2Vec2 r1 = ropeAt(u1);
                draw::line(r0, r1, 0.06f, kInk);
                draw::line(r0, r1, 0.036f, ropeColor);
            }
            for (size_t pi = 0; pi < b.planks.size(); ++pi) {
                const Terrain::BridgePlank& plank = b.planks[pi];
                if (!b2Body_IsValid(plank.body)) continue;
                const float u = (static_cast<float>(pi) + 0.5f) / static_cast<float>(b.planks.size());
                const b2Vec2 ropeTop = ropeAt(u);
                const b2Vec2 plankPos = b2Body_GetTransform(plank.body).p;
                draw::line(ropeTop, plankPos, 0.025f, draw::withAlpha(ropeColor, 0.65f));
            }

            // Wooden support posts anchored at both creek banks.
            auto drawPost = [&](float px, float py) {
                draw::line({px, py - postDepth}, {px, py + postH}, 0.24f, kInk);
                draw::line({px, py - postDepth}, {px, py + postH}, 0.17f, postWood);
                draw::circleOutlined({px, py + postH + 0.05f}, 0.13f, timberLight, kInk, 0.04f);
            };
            drawPost(b.x0, b.y0);
            drawPost(b.x1, b.y1);

            // Live articulated bridge planks from Box2D bodies.
            for (size_t pi = 0; pi < b.planks.size(); ++pi) {
                const Terrain::BridgePlank& plank = b.planks[pi];
                if (!b2Body_IsValid(plank.body)) continue;
                const b2Transform xf = b2Body_GetTransform(plank.body);
                const b2Vec2 c0 = b2TransformPoint(xf, {-plank.halfW, -plank.halfH});
                const b2Vec2 c1 = b2TransformPoint(xf, {plank.halfW, -plank.halfH});
                const b2Vec2 c2 = b2TransformPoint(xf, {plank.halfW, plank.halfH});
                const b2Vec2 c3 = b2TransformPoint(xf, {-plank.halfW, plank.halfH});
                const Color wood = (pi % 2 == 0) ? timberLight : timberDark;
                const Vector2 v0 = draw::rv(c0);
                const Vector2 v1 = draw::rv(c1);
                const Vector2 v2 = draw::rv(c2);
                const Vector2 v3 = draw::rv(c3);
                DrawTriangle(v0, v1, v2, wood);
                DrawTriangle(v0, v2, v3, wood);
                draw::line(c0, c1, 0.045f, kInk);
                draw::line(c1, c2, 0.045f, kInk);
                draw::line(c2, c3, 0.045f, kInk);
                draw::line(c3, c0, 0.045f, kInk);
                // Timber grain & bolt/nail dots.
                draw::circle(b2TransformPoint(xf, {-plank.halfW * 0.62f, 0.0f}), 0.032f, kInk);
                draw::circle(b2TransformPoint(xf, {plank.halfW * 0.62f, 0.0f}), 0.032f, kInk);
            }
        }
    }
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
    drawBridges(session, pal);
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
        const Terrain& t = session.terrain();
        const TerrainGenerator& gen = t.generator();
        for (float x = x0; x < x1; x += Terrain::kStep) {
            const float midX = x + Terrain::kStep * 0.5f;
            const Color lineColor = gen.isIce(midX) ? SKYBLUE : (gen.isMud(midX) ? BROWN : RED);
            DrawLineEx(draw::rv(x, t.surfaceHeight(x)), draw::rv(x + Terrain::kStep, t.surfaceHeight(x + Terrain::kStep)),
                       0.04f, lineColor);
            if (gen.hasCeiling()) {
                DrawLineEx(draw::rv(x, t.ceilingHeight(x)),
                           draw::rv(x + Terrain::kStep, t.ceilingHeight(x + Terrain::kStep)), 0.04f, ORANGE);
            }
        }
        for (const auto& [chunk, list] : t.bridges()) {
            (void)chunk;
            for (const Terrain::Bridge& b : list) {
                for (const Terrain::BridgePlank& plank : b.planks) {
                    if (!b2Body_IsValid(plank.body)) continue;
                    const b2Transform xf = b2Body_GetTransform(plank.body);
                    const Vector2 q0 = draw::rv(b2TransformPoint(xf, {-plank.halfW, -plank.halfH}));
                    const Vector2 q1 = draw::rv(b2TransformPoint(xf, {plank.halfW, -plank.halfH}));
                    const Vector2 q2 = draw::rv(b2TransformPoint(xf, {plank.halfW, plank.halfH}));
                    const Vector2 q3 = draw::rv(b2TransformPoint(xf, {-plank.halfW, plank.halfH}));
                    DrawLineEx(q0, q1, 0.04f, YELLOW);
                    DrawLineEx(q1, q2, 0.04f, YELLOW);
                    DrawLineEx(q2, q3, 0.04f, YELLOW);
                    DrawLineEx(q3, q0, 0.04f, YELLOW);
                }
            }
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

    if (pal.cave) {
        // Subtle subterranean ambient depth blooms and faint distant crystal twinkles.
        DrawCircleV({w * 0.25f, h * 0.30f}, h * 0.34f, draw::withAlpha({70, 96, 138, 255}, 0.09f));
        DrawCircleV({w * 0.72f, h * 0.42f}, h * 0.38f, draw::withAlpha({92, 72, 128, 255}, 0.08f));
        Rng rng(0xCA7E77ull);
        for (int i = 0; i < 48; ++i) {
            const float x = std::fmod(rng.range(0.0f, w * 2.0f) - centre_.x * 3.0f, w * 2.0f);
            const float sx = x < 0.0f ? x + w * 2.0f : x;
            const float sy = rng.range(h * 0.06f, h * 0.72f);
            const float twinkle = 0.25f + 0.55f * (0.5f + 0.5f * std::sin(time * rng.range(1.2f, 3.2f) + static_cast<float>(i)));
            const Color glow = (i % 3 == 0) ? Color{240, 185, 85, 255} : Color{95, 220, 245, 255};
            DrawCircleV({sx - w * 0.5f, sy}, rng.range(1.0f, 2.4f) * (h / 720.0f), draw::withAlpha(glow, twinkle));
        }
        return;
    }

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
            case Scenery::Cave: {
                const Color glow = rng.chance(0.55f) ? Color{85, 220, 245, 210} : Color{245, 185, 80, 210};
                drawStalagmite(base, size * 0.95f, dark, glow, rng.chance(0.45f));
                break;
            }
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

    // Surface strip (grass / sand / snow / mud), following the normals.
    const float thick = 0.48f;
    rlSetTexture(assets.surfaceTexture.id);
    rlBegin(RL_QUADS);
    for (int k = 0; k + 1 < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(k)];
        const b2Vec2 b = top[static_cast<size_t>(k + 1)];
        const b2Vec2 na = normal[static_cast<size_t>(k)];
        const b2Vec2 nb = normal[static_cast<size_t>(k + 1)];
        const float midX = (a.x + b.x) * 0.5f;
        const bool ice = gen.isIce(midX);
        const bool mud = gen.isMud(midX);
        const Color c = ice ? pal.ice : (mud ? pal.mud : pal.surface);
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

    // Darker underside of the strip and a crisp top edge (plus ice sheen / wet mud ripples).
    for (int k = 0; k + 1 < n; ++k) {
        const b2Vec2 a = top[static_cast<size_t>(k)];
        const b2Vec2 b = top[static_cast<size_t>(k + 1)];
        const b2Vec2 na = normal[static_cast<size_t>(k)];
        const b2Vec2 nb = normal[static_cast<size_t>(k + 1)];
        const float midX = (a.x + b.x) * 0.5f;
        const bool ice = gen.isIce(midX);
        const bool mud = gen.isMud(midX);
        const Color edge = ice ? draw::shade(pal.ice, 0.8f) : (mud ? draw::shade(pal.mud, 0.70f) : pal.surfaceEdge);
        const Vector2 q[4] = {draw::rv(b2MulAdd(a, -thick * 0.78f, na)), draw::rv(b2MulAdd(a, -thick, na)),
                              draw::rv(b2MulAdd(b, -thick, nb)), draw::rv(b2MulAdd(b, -thick * 0.78f, nb))};
        DrawTriangle(q[0], q[1], q[2], edge);
        DrawTriangle(q[0], q[2], q[3], edge);
        DrawLineEx(draw::rv(a), draw::rv(b), 0.06f, draw::withAlpha(kInk, 0.55f));
        const int sampleIdx = i0 + k;
        if (ice && sampleIdx % 6 == 0) {
            DrawLineEx(draw::rv(b2MulAdd(a, -0.12f, na)), draw::rv(b2MulAdd(b, -0.12f, nb)), 0.05f, draw::withAlpha(WHITE, 0.8f));
        } else if (mud && (sampleIdx % 5 == 0 || sampleIdx % 7 == 0)) {
            const float depth = (sampleIdx % 5 == 0) ? -0.11f : -0.23f;
            DrawLineEx(draw::rv(b2MulAdd(a, depth, na)), draw::rv(b2MulAdd(b, depth, nb)), 0.045f,
                       draw::withAlpha({146, 108, 78, 255}, 0.6f));
        }
    }

    // Cave ceiling: textured rock mass above ceilingHeight(x), rugged rim, stalactites, and hanging lights.
    if (gen.hasCeiling()) {
        const float topY = centre_.y + halfH + 2.0f;
        std::vector<b2Vec2> ceilPts(static_cast<size_t>(n));
        std::vector<b2Vec2> ceilNorm(static_cast<size_t>(n));
        for (int k = 0; k < n; ++k) {
            const float x = Terrain::sampleX(i0 + k);
            ceilPts[static_cast<size_t>(k)] = {x, gen.ceilingHeight(x)};
        }
        for (int k = 0; k < n; ++k) {
            const b2Vec2 ca = ceilPts[static_cast<size_t>(std::max(0, k - 1))];
            const b2Vec2 cb = ceilPts[static_cast<size_t>(std::min(n - 1, k + 1))];
            const b2Vec2 tangent = b2Normalize(b2Sub(cb, ca));
            ceilNorm[static_cast<size_t>(k)] = {-tangent.y, tangent.x};  // points up into ceiling rock
        }

        const Color deepCeil = draw::shade(pal.ceiling, 0.58f);
        rlSetTexture(assets.groundTexture.id);
        rlBegin(RL_QUADS);
        for (int k = 0; k + 1 < n; ++k) {
            const b2Vec2 ca = ceilPts[static_cast<size_t>(k)];
            const b2Vec2 cb = ceilPts[static_cast<size_t>(k + 1)];
            const float band = 5.5f;
            // Band 1: ceiling surface up 5.5 m into rock, darkening upward.
            rlColor4ub(pal.ceiling.r, pal.ceiling.g, pal.ceiling.b, 255);
            rlTexCoord2f(ca.x / tile, -ca.y / tile);
            rlVertex2f(ca.x, -ca.y);
            rlColor4ub(deepCeil.r, deepCeil.g, deepCeil.b, 255);
            rlTexCoord2f(ca.x / tile, -(ca.y + band) / tile);
            rlVertex2f(ca.x, -(ca.y + band));
            rlTexCoord2f(cb.x / tile, -(cb.y + band) / tile);
            rlVertex2f(cb.x, -(cb.y + band));
            rlColor4ub(pal.ceiling.r, pal.ceiling.g, pal.ceiling.b, 255);
            rlTexCoord2f(cb.x / tile, -cb.y / tile);
            rlVertex2f(cb.x, -cb.y);
            // Band 2: solid up to topY.
            if (std::fmin(ca.y, cb.y) + band < topY) {
                rlColor4ub(deepCeil.r, deepCeil.g, deepCeil.b, 255);
                rlTexCoord2f(ca.x / tile, -(ca.y + band) / tile);
                rlVertex2f(ca.x, -(ca.y + band));
                rlTexCoord2f(ca.x / tile, -topY / tile);
                rlVertex2f(ca.x, -topY);
                rlTexCoord2f(cb.x / tile, -topY / tile);
                rlVertex2f(cb.x, -topY);
                rlTexCoord2f(cb.x / tile, -(cb.y + band) / tile);
                rlVertex2f(cb.x, -(cb.y + band));
            }
        }
        rlEnd();
        rlSetTexture(0);

        // Rugged underside rim along the ceiling + crisp dark ink outline.
        const float ceilThick = 0.42f;
        for (int k = 0; k + 1 < n; ++k) {
            const b2Vec2 ca = ceilPts[static_cast<size_t>(k)];
            const b2Vec2 cb = ceilPts[static_cast<size_t>(k + 1)];
            const b2Vec2 cna = ceilNorm[static_cast<size_t>(k)];
            const b2Vec2 cnb = ceilNorm[static_cast<size_t>(k + 1)];
            const Vector2 rq[4] = {draw::rv(ca), draw::rv(b2MulAdd(ca, ceilThick, cna)),
                                   draw::rv(b2MulAdd(cb, ceilThick, cnb)), draw::rv(cb)};
            DrawTriangle(rq[0], rq[1], rq[2], pal.ceilingEdge);
            DrawTriangle(rq[0], rq[2], rq[3], pal.ceilingEdge);
            DrawLineEx(draw::rv(ca), draw::rv(cb), 0.08f, draw::withAlpha(kInk, 0.85f));
        }

        // Deterministic hanging stalactites and warm/cyan cave lanterns/crystals along the ceiling.
        const float stalSpacing = 2.6f;
        const float minX = Terrain::sampleX(i0);
        const float maxX = Terrain::sampleX(i1);
        for (float slot = std::floor(minX / stalSpacing); slot * stalSpacing < maxX; slot += 1.0f) {
            Rng rng(hashCombine(session.stage().seed ^ 0xCA7E57A1ull, static_cast<uint64_t>(static_cast<int64_t>(slot))));
            if (!rng.chance(0.70f)) continue;
            const float x = slot * stalSpacing + rng.range(0.15f, stalSpacing - 0.15f);
            const float cy = terrain.ceilingHeight(x) + 0.12f;
            const float drop = rng.range(0.45f, 1.05f);
            const float halfBase = drop * rng.range(0.28f, 0.42f);
            const Vector2 left = draw::rv(x - halfBase, cy);
            const Vector2 right = draw::rv(x + halfBase, cy);
            const Vector2 tip = draw::rv(x + rng.range(-0.08f, 0.08f), cy - drop);
            const Vector2 midBase = draw::rv(x, cy);
            DrawTriangle(left, tip, right, pal.ceilingEdge);
            DrawTriangle(midBase, tip, right, draw::shade(pal.ceilingEdge, 0.78f));
            DrawLineEx(left, tip, 0.05f, draw::withAlpha(kInk, 0.8f));
            DrawLineEx(tip, right, 0.05f, draw::withAlpha(kInk, 0.8f));
        }

        const float lampSpacing = 9.5f;
        for (float slot = std::floor(minX / lampSpacing); slot * lampSpacing < maxX; slot += 1.0f) {
            if (slot * lampSpacing < 8.0f && slot * lampSpacing > -15.0f) continue;
            Rng rng(hashCombine(session.stage().seed ^ 0xCA7E1A39ull, static_cast<uint64_t>(static_cast<int64_t>(slot))));
            if (!rng.chance(0.85f)) continue;
            const float x = slot * lampSpacing + rng.range(1.0f, lampSpacing - 1.0f);
            const float cy = terrain.ceilingHeight(x) + 0.06f;
            if (rng.chance(0.50f)) {
                // Small warm hanging cave lantern.
                const b2Vec2 anchor{x, cy};
                const b2Vec2 bulb{x, cy - 0.55f};
                draw::line(anchor, bulb, 0.04f, kInk);
                draw::circle(bulb, 0.85f, draw::withAlpha({255, 196, 90, 255}, 0.16f));
                draw::circle(bulb, 0.42f, draw::withAlpha({255, 220, 130, 255}, 0.28f));
                draw::circleOutlined(bulb, 0.14f, {255, 232, 150, 255}, kInk, 0.04f);
            } else {
                // Hanging glowing ceiling crystal cluster.
                const Color crys = rng.chance(0.55f) ? Color{85, 225, 250, 255} : Color{250, 185, 75, 255};
                const b2Vec2 center{x, cy - 0.30f};
                draw::circle(center, 0.90f, draw::withAlpha(crys, 0.16f));
                const Vector2 pL = draw::rv(x - 0.18f, cy);
                const Vector2 pR = draw::rv(x + 0.18f, cy);
                const Vector2 pTip = draw::rv(x, cy - 0.65f);
                DrawTriangle(pL, pTip, pR, crys);
                DrawTriangle(draw::rv(x, cy), pTip, pR, draw::shade(crys, 0.78f));
                DrawLineEx(pL, pTip, 0.04f, kInk);
                DrawLineEx(pTip, pR, 0.04f, kInk);
            }
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
        if (terrain.generator().isIce(x) || terrain.generator().isMud(x)) continue;
        if (terrain.bridgeDeckHeight(x) > -1e5f) continue;
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
        case Scenery::Cave: {
            const Color glow = rng.chance(0.55f) ? Color{85, 225, 250, 255} : Color{250, 185, 75, 255};
            if (roll < 0.35f) drawStalagmite(at, rng.range(0.45f, 0.85f), pal.surfaceEdge, glow, true);
            else if (roll < 0.75f) drawStalagmite(at, rng.range(0.40f, 0.80f), draw::shade(pal.surfaceEdge, 0.9f), glow, false);
            else drawRock(at, rng.range(0.30f, 0.50f), pal.surfaceEdge);
            break;
        }
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
    if (pal.cave) {
        Rng rng(0xCA7E55ull);
        for (int i = 0; i < 18; ++i) {
            const Color c = (i % 3 == 0) ? Color{240, 185, 85, 160} : Color{95, 220, 245, 160};
            DrawCircleV({r.x + rng.range(0.0f, r.width), r.y + rng.range(r.height * 0.22f, r.height * 0.65f)}, 1.4f, c);
        }
    } else {
        if (pal.stars) {
            Rng rng(5);
            for (int i = 0; i < 30; ++i) DrawCircleV({r.x + rng.range(0.0f, r.width), r.y + rng.range(0.0f, r.height * 0.6f)}, 1.2f, WHITE);
        }
        if (pal.planet) DrawCircleV({r.x + r.width * 0.78f, r.y + r.height * 0.22f}, r.height * 0.11f, {60, 120, 220, 255});
        else DrawCircleV({r.x + r.width * 0.78f, r.y + r.height * 0.22f}, r.height * 0.09f, pal.sun);
    }

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

    if (stage.feature == TerrainFeature::CaveCeiling) {
        strip.clear();
        for (float sx = 0.0f; sx <= r.width + 4.0f; sx += 4.0f) {
            const float wx = 400.0f + sx / r.width * 80.0f;
            const float y = r.y + r.height * 0.24f - far.fbm(wx * 0.08f + 3.0f, 3) * r.height * 0.08f;
            strip.push_back({r.x + sx, r.y - 2.0f});
            strip.push_back({r.x + sx, y});
        }
        DrawTriangleStrip(strip.data(), static_cast<int>(strip.size()), pal.ceilingEdge);
        for (size_t i = 1; i < strip.size(); i += 2) strip[i].y -= r.height * 0.04f;
        DrawTriangleStrip(strip.data(), static_cast<int>(strip.size()), pal.ceiling);

        Rng rng(stage.seed ^ 0xCA7E57u);
        for (int i = 0; i < 7; ++i) {
            const float sx = r.x + r.width * (0.08f + 0.14f * static_cast<float>(i)) + rng.range(-4.0f, 4.0f);
            const float wx = 400.0f + (sx - r.x) / r.width * 80.0f;
            const float ceilY = r.y + r.height * 0.24f - far.fbm(wx * 0.08f + 3.0f, 3) * r.height * 0.08f;
            const float baseY = ceilY - r.height * 0.02f;
            const float drop = r.height * rng.range(0.08f, 0.15f);
            const float hw = drop * 0.36f;
            DrawTriangle({sx - hw, baseY}, {sx, baseY + drop}, {sx + hw, baseY}, pal.ceilingEdge);
            if (i % 3 == 1) {
                const Color glow = (i == 1) ? Color{85, 225, 250, 230} : Color{250, 185, 75, 230};
                DrawCircleV({sx + hw * 1.4f, baseY + drop * 0.35f}, 2.4f, glow);
            }
        }
    }
    EndScissorMode();
}

} // namespace hr
