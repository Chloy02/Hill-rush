#include "game/VehicleArt.hpp"

#include "core/Math.hpp"
#include "game/Draw.hpp"

#include <cmath>
#include <map>
#include <string>

namespace hr {

namespace {

using draw::Shape;

constexpr float kLine = 0.045f;
const Color kInk{36, 26, 34, 255};
const Color kSkin{244, 196, 154, 255};
const Color kSkinShade{214, 160, 120, 255};
const Color kTire{44, 44, 50, 255};
const Color kSteel{150, 156, 166, 255};
const Color kSteelDark{96, 100, 112, 255};

// --- shape building helpers ---------------------------------------------------

void appendArc(std::vector<b2Vec2>& pts, b2Vec2 c, float r, float a0, float a1, int segments)
{
    for (int i = 0; i <= segments; ++i) {
        const float a = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(segments);
        pts.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
}

// Adds a wheel arch cut into a body bottom edge at height `bottom`, walking left to right.
void appendArch(std::vector<b2Vec2>& pts, b2Vec2 wheel, float r, float bottom)
{
    const float dy = bottom - wheel.y;
    const float dx = std::sqrt(std::fmax(0.0f, r * r - dy * dy));
    const float left = std::atan2(dy, -dx);
    const float right = std::atan2(dy, dx);
    appendArc(pts, wheel, r, left, right, 14);
}

std::vector<b2Vec2> roundedRect(float x0, float y0, float x1, float y1, float r)
{
    std::vector<b2Vec2> pts;
    appendArc(pts, {x1 - r, y0 + r}, r, -kPi * 0.5f, 0.0f, 4);
    appendArc(pts, {x1 - r, y1 - r}, r, 0.0f, kPi * 0.5f, 4);
    appendArc(pts, {x0 + r, y1 - r}, r, kPi * 0.5f, kPi, 4);
    appendArc(pts, {x0 + r, y0 + r}, r, kPi, kPi * 1.5f, 4);
    return pts;
}

std::vector<b2Vec2> ring(b2Vec2 c, float r0, float r1, float a0, float a1, int segments)
{
    std::vector<b2Vec2> pts;
    appendArc(pts, c, r1, a0, a1, segments);
    std::vector<b2Vec2> inner;
    appendArc(inner, c, r0, a0, a1, segments);
    pts.insert(pts.end(), inner.rbegin(), inner.rend());
    return pts;
}

b2Transform compose(b2Transform parent, b2Vec2 offset)
{
    return {b2TransformPoint(parent, offset), parent.q};
}

// --- shared parts ---------------------------------------------------------------

struct WheelStyle {
    Color tire = kTire;
    Color rim = {206, 210, 220, 255};
    Color hub = {110, 114, 126, 255};
    int knobs = 14;
    float knobDepth = 0.06f;  // fraction of radius
    float rimScale = 0.56f;
    int spokes = 5;
    bool slick = false;
    bool wireMesh = false;
};

void drawWheel(b2Transform t, float r, const WheelStyle& s)
{
    const b2Vec2 c = t.p;
    // Tyre with tread blocks.
    draw::circle(c, r + kLine * 0.5f, kInk);
    if (!s.slick) {
        const float knobW = (kTwoPi * r / static_cast<float>(s.knobs)) * 0.45f;
        for (int i = 0; i < s.knobs; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(s.knobs) * kTwoPi;
            const b2Vec2 dir = b2RotateVector(t.q, {std::cos(a), std::sin(a)});
            const b2Vec2 p0 = b2MulAdd(c, r * (1.0f - s.knobDepth), dir);
            const b2Vec2 p1 = b2MulAdd(c, r + r * s.knobDepth * 0.6f, dir);
            draw::line(p0, p1, knobW + kLine, kInk);
            draw::line(p0, p1, knobW, s.tire);
        }
    }
    draw::circle(c, r - kLine * 0.5f, s.tire);
    // Sidewall highlight (or chevron lunar mesh band).
    DrawRing(draw::rv(c), r * 0.70f, r * 0.80f, 0.0f, 360.0f, 32, draw::shade(s.tire, 1.35f));
    if (s.wireMesh) {
        for (int i = 0; i < s.knobs; ++i) {
            const float a0 = static_cast<float>(i) / static_cast<float>(s.knobs) * kTwoPi;
            const float a1 = static_cast<float>(i + 1) / static_cast<float>(s.knobs) * kTwoPi;
            const b2Vec2 d0 = b2RotateVector(t.q, {std::cos(a0), std::sin(a0)});
            const b2Vec2 d1 = b2RotateVector(t.q, {std::cos(a1), std::sin(a1)});
            draw::line(b2MulAdd(c, r * 0.66f, d0), b2MulAdd(c, r * 0.92f, d1), r * 0.045f, s.rim);
        }
    }
    // Rim and spokes.
    const float rr = r * s.rimScale;
    draw::circle(c, rr + kLine * 0.5f, kInk);
    draw::circle(c, rr, s.rim);
    DrawRing(draw::rv(c), rr * 0.72f, rr * 0.86f, 0.0f, 360.0f, 32, draw::shade(s.rim, 0.85f));
    if (s.wireMesh) {
        for (int i = 0; i < s.spokes; ++i) {
            const float a0 = static_cast<float>(i) / static_cast<float>(s.spokes) * kTwoPi;
            const float a1 = a0 + kPi * 0.35f;
            const b2Vec2 d0 = b2RotateVector(t.q, {std::cos(a0), std::sin(a0)});
            const b2Vec2 d1 = b2RotateVector(t.q, {std::cos(a1), std::sin(a1)});
            draw::line(b2MulAdd(c, rr * 0.24f, d0), b2MulAdd(c, rr * 0.80f, d1), rr * 0.09f, draw::shade(s.rim, 0.68f));
        }
    } else {
        for (int i = 0; i < s.spokes; ++i) {
            const float a = static_cast<float>(i) / static_cast<float>(s.spokes) * kTwoPi;
            const b2Vec2 dir = b2RotateVector(t.q, {std::cos(a), std::sin(a)});
            draw::line(b2MulAdd(c, rr * 0.25f, dir), b2MulAdd(c, rr * 0.78f, dir), rr * 0.16f, draw::shade(s.rim, 0.75f));
        }
    }
    draw::circle(c, rr * 0.26f, s.hub);
    draw::circle(c, rr * 0.10f, kInk);
}

// Spring + damper between the chassis mount and the wheel hub.
void drawStrut(b2Vec2 mount, b2Vec2 hub, float width, Color body, bool spring)
{
    draw::line(mount, hub, width + kLine, kInk);
    draw::line(mount, hub, width, body);
    if (!spring) return;
    const b2Vec2 d = b2Sub(hub, mount);
    const float len = b2Length(d);
    if (len < 0.05f) return;
    const b2Vec2 u = b2MulSV(1.0f / len, d);
    const b2Vec2 n{-u.y, u.x};
    const int coils = 6;
    b2Vec2 prev = b2MulAdd(mount, len * 0.12f, u);
    for (int i = 1; i <= coils * 2; ++i) {
        const float t = 0.12f + 0.76f * static_cast<float>(i) / static_cast<float>(coils * 2);
        const float side = (i % 2 == 0 ? -1.0f : 1.0f) * width * 1.15f;
        const b2Vec2 p = b2MulAdd(b2MulAdd(mount, len * t, u), side, n);
        draw::line(prev, p, width * 0.38f, {230, 206, 60, 255});
        prev = p;
    }
}

enum class Headgear { Helmet, FullFace, StrawHat, RaceHelmet };

void drawHead(b2Transform t, float r, Headgear gear, Color gearColor, Color accent, bool crashed, bool goldVisor = false,
              Color accent2 = BLANK)
{
    const b2Vec2 c = t.p;
    auto P = [&](float x, float y) { return b2TransformPoint(t, {x * r, y * r}); };

    // Face.
    draw::circle(c, r + kLine * 0.5f, kInk);
    draw::circle(c, r - kLine * 0.5f, kSkin);
    // Nose and ear.
    draw::circle(P(0.92f, -0.08f), r * 0.20f, kSkinShade);
    draw::circle(P(-0.30f, -0.05f), r * 0.17f, kSkinShade);

    if (gear == Headgear::StrawHat) {
        draw::line(P(-1.25f, 0.42f), P(1.30f, 0.42f), r * 0.22f, kInk);
        draw::line(P(-1.20f, 0.42f), P(1.25f, 0.42f), r * 0.14f, gearColor);
        Shape crown(std::vector<b2Vec2>{{-0.70f * r, 0.42f * r}, {0.70f * r, 0.42f * r}, {0.55f * r, 1.05f * r},
                                        {-0.55f * r, 1.05f * r}});
        draw::fillOutlined(crown, t, gearColor, kInk, kLine);
        draw::line(P(-0.66f, 0.55f), P(0.66f, 0.55f), r * 0.14f, accent);
    } else {
        // Dome covering the top of the head.
        std::vector<b2Vec2> dome;
        const float top = gear == Headgear::FullFace ? 1.12f : 1.08f;
        appendArc(dome, {0.0f, 0.0f}, top * r, gear == Headgear::FullFace ? -0.75f : -0.05f, kPi + 0.25f, 18);
        if (gear == Headgear::FullFace) {
            dome.push_back({-0.95f * r, -0.70f * r});
            dome.push_back({0.30f * r, -1.02f * r});
        } else {
            dome.push_back({-1.12f * r, -0.05f * r});
            dome.push_back({0.85f * r, 0.18f * r});
        }
        Shape helmet(dome);
        draw::fillOutlined(helmet, t, gearColor, kInk, kLine);
        // Stripe over the top.
        draw::line(P(-0.55f, 0.80f), P(0.45f, 0.92f), r * 0.20f, accent);
        if (accent2.a > 0) draw::line(P(-0.52f, 0.64f), P(0.42f, 0.76f), r * 0.11f, accent2);
        if (gear == Headgear::FullFace || gear == Headgear::RaceHelmet) {
            Shape visor(std::vector<b2Vec2>{{0.15f * r, 0.12f * r}, {1.12f * r, 0.18f * r}, {1.05f * r, -0.30f * r},
                                            {0.20f * r, -0.28f * r}});
            const Color visorFill = goldVisor ? Color{234, 178, 38, 255} : Color{46, 62, 96, 255};
            const Color glint = goldVisor ? Color{255, 246, 175, 235} : Color{150, 190, 240, 200};
            draw::fillOutlined(visor, t, visorFill, kInk, kLine * 0.8f);
            draw::line(P(0.35f, 0.04f), P(0.85f, 0.08f), r * 0.10f, glint);
            return;  // face hidden behind the visor
        }
    }

    // Eye.
    const b2Vec2 eye = P(0.48f, 0.12f);
    if (crashed) {
        const float e = r * 0.16f;
        const b2Vec2 dx = b2RotateVector(t.q, {e, e});
        const b2Vec2 dy = b2RotateVector(t.q, {e, -e});
        draw::line(b2Sub(eye, dx), b2Add(eye, dx), r * 0.09f, kInk);
        draw::line(b2Sub(eye, dy), b2Add(eye, dy), r * 0.09f, kInk);
    } else {
        draw::circle(eye, r * 0.19f, WHITE);
        draw::circle(P(0.56f, 0.10f), r * 0.10f, kInk);
    }
    // Mouth.
    if (crashed) draw::circle(P(0.55f, -0.45f), r * 0.11f, kInk);
    else draw::line(P(0.38f, -0.45f), P(0.70f, -0.40f), r * 0.08f, kInk);
}

// --- per-vehicle art --------------------------------------------------------------

struct JeepArt {
    Shape body, seat, glass, torso, bumperRear, bumperFront, door;
};

const JeepArt& jeepArt()
{
    static const JeepArt art = [] {
        JeepArt a;
        std::vector<b2Vec2> body{{-1.47f, -0.16f}};
        appendArch(body, {-0.98f, -0.40f}, 0.53f, -0.16f);
        appendArch(body, {0.98f, -0.40f}, 0.53f, -0.16f);
        for (b2Vec2 p : {b2Vec2{1.47f, -0.16f}, b2Vec2{1.52f, -0.04f}, b2Vec2{1.52f, 0.20f}, b2Vec2{1.42f, 0.29f},
                         b2Vec2{0.32f, 0.34f}, b2Vec2{0.18f, 0.30f}, b2Vec2{-1.47f, 0.30f}})
            body.push_back(p);
        a.body = Shape(body);
        a.seat = Shape(roundedRect(-0.70f, 0.20f, -0.46f, 0.78f, 0.08f));
        a.glass = Shape(std::vector<b2Vec2>{{0.12f, 0.33f}, {0.24f, 0.33f}, {0.10f, 0.84f}, {0.02f, 0.84f}});
        a.torso = Shape(std::vector<b2Vec2>{{-0.52f, 0.22f}, {-0.10f, 0.22f}, {-0.12f, 0.58f}, {-0.22f, 0.80f},
                                            {-0.40f, 0.84f}, {-0.52f, 0.66f}});
        a.bumperRear = Shape(roundedRect(-1.60f, -0.18f, -1.42f, 0.06f, 0.05f));
        a.bumperFront = Shape(roundedRect(1.44f, -0.18f, 1.62f, 0.04f, 0.05f));
        a.door = Shape(roundedRect(-0.42f, -0.10f, 0.20f, 0.24f, 0.06f));
        return a;
    }();
    return art;
}

void drawJeep(const VehicleDef& def, const VehiclePose& pose)
{
    const JeepArt& a = jeepArt();
    const b2Transform t = pose.chassis;
    const Color red{214, 56, 48, 255};
    const Color redDark{164, 34, 30, 255};

    // Spare wheel on the back.
    WheelStyle spare;
    spare.slick = true;
    spare.rimScale = 0.5f;
    drawWheel(compose(t, {-1.62f, 0.22f}), 0.26f, spare);

    // Roll bar.
    const b2Vec2 rb0 = b2TransformPoint(t, {-0.94f, 0.28f});
    const b2Vec2 rb1 = b2TransformPoint(t, {-0.86f, 0.98f});
    const b2Vec2 rb2 = b2TransformPoint(t, {-0.58f, 1.02f});
    draw::line(rb0, rb1, 0.11f + kLine, kInk);
    draw::line(rb1, rb2, 0.11f + kLine, kInk);
    draw::line(rb0, rb1, 0.11f, kSteel);
    draw::line(rb1, rb2, 0.11f, kSteel);

    draw::fillOutlined(a.seat, t, {58, 52, 60, 255}, kInk, kLine);
    // Driver.
    draw::fillOutlined(a.torso, t, {58, 112, 214, 255}, kInk, kLine);
    // Steering column and wheel.
    draw::line(b2TransformPoint(t, {0.10f, 0.30f}), b2TransformPoint(t, {0.00f, 0.58f}), 0.06f, kInk);
    draw::line(b2TransformPoint(t, {-0.06f, 0.68f}), b2TransformPoint(t, {0.08f, 0.46f}), 0.07f, kInk);
    // Arm reaching for the wheel.
    const b2Vec2 shoulder = b2TransformPoint(t, {-0.30f, 0.70f});
    const b2Vec2 elbow = b2TransformPoint(t, {-0.12f, 0.50f});
    const b2Vec2 hand = b2TransformPoint(t, {0.02f, 0.60f});
    draw::line(shoulder, elbow, 0.13f + kLine, kInk);
    draw::line(elbow, hand, 0.11f + kLine, kInk);
    draw::line(shoulder, elbow, 0.13f, {48, 96, 196, 255});
    draw::line(elbow, hand, 0.11f, kSkin);

    // Body.
    draw::fillOutlined(a.body, t, red, kInk, kLine);
    draw::line(b2TransformPoint(t, {-1.40f, 0.22f}), b2TransformPoint(t, {0.20f, 0.24f}), 0.06f, {240, 110, 100, 255});
    draw::outline(a.door, t, 0.03f, redDark);
    draw::line(b2TransformPoint(t, {-0.05f, 0.12f}), b2TransformPoint(t, {0.10f, 0.12f}), 0.04f, kInk);
    draw::fillOutlined(a.bumperRear, t, {70, 70, 80, 255}, kInk, kLine);
    draw::fillOutlined(a.bumperFront, t, {70, 70, 80, 255}, kInk, kLine);
    // Headlight and tail light.
    draw::circleOutlined(b2TransformPoint(t, {1.46f, 0.14f}), 0.07f, {255, 236, 140, 255}, kInk, kLine * 0.8f);
    draw::circleOutlined(b2TransformPoint(t, {-1.44f, 0.18f}), 0.05f, {255, 80, 60, 255}, kInk, kLine * 0.8f);
    // Windshield.
    draw::fillOutlined(a.glass, t, {170, 220, 250, 160}, kInk, kLine);
    draw::line(b2TransformPoint(t, {0.24f, 0.33f}), b2TransformPoint(t, {0.10f, 0.86f}), 0.06f, {70, 70, 80, 255});
    (void)def;
}

struct BikeArt {
    Shape frame, engine, seat, plate, torso, tank;
};

const BikeArt& bikeArt()
{
    static const BikeArt art = [] {
        BikeArt a;
        a.frame = Shape(std::vector<b2Vec2>{{-0.70f, 0.10f}, {-0.20f, -0.06f}, {0.40f, -0.06f}, {0.62f, 0.30f},
                                            {0.52f, 0.40f}, {0.00f, 0.30f}, {-0.70f, 0.30f}});
        a.engine = Shape(roundedRect(-0.20f, -0.22f, 0.26f, 0.14f, 0.07f));
        a.seat = Shape(std::vector<b2Vec2>{{-0.78f, 0.30f}, {0.02f, 0.32f}, {0.08f, 0.42f}, {-0.70f, 0.42f}});
        a.plate = Shape(std::vector<b2Vec2>{{0.52f, 0.34f}, {0.70f, 0.30f}, {0.80f, 0.58f}, {0.64f, 0.62f}});
        a.torso = Shape(std::vector<b2Vec2>{{-0.30f, 0.40f}, {0.02f, 0.40f}, {0.26f, 0.84f}, {0.10f, 0.98f},
                                            {-0.06f, 0.92f}});
        a.tank = Shape(std::vector<b2Vec2>{{0.00f, 0.30f}, {0.42f, 0.32f}, {0.50f, 0.42f}, {0.10f, 0.48f}});
        return a;
    }();
    return art;
}

void drawBike(const VehicleDef& def, const VehiclePose& pose)
{
    const BikeArt& a = bikeArt();
    const b2Transform t = pose.chassis;
    const Color orange{246, 132, 36, 255};

    // Swingarm to the rear hub and fork to the front hub.
    if (pose.wheels.size() >= 2) {
        drawStrut(b2TransformPoint(t, {-0.10f, 0.02f}), pose.wheels[0].p, 0.10f, kSteelDark, false);
        drawStrut(b2TransformPoint(t, {0.55f, 0.36f}), pose.wheels[1].p, 0.09f, kSteel, false);
    }
    // Exhaust pipe.
    draw::line(b2TransformPoint(t, {0.10f, -0.10f}), b2TransformPoint(t, {-0.40f, 0.10f}), 0.10f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-0.40f, 0.10f}), b2TransformPoint(t, {-0.86f, 0.16f}), 0.12f + kLine, kInk);
    draw::line(b2TransformPoint(t, {0.10f, -0.10f}), b2TransformPoint(t, {-0.40f, 0.10f}), 0.10f, kSteel);
    draw::line(b2TransformPoint(t, {-0.40f, 0.10f}), b2TransformPoint(t, {-0.86f, 0.16f}), 0.12f, kSteel);

    draw::fillOutlined(a.engine, t, {74, 74, 84, 255}, kInk, kLine);
    draw::fillOutlined(a.frame, t, orange, kInk, kLine);
    draw::fillOutlined(a.tank, t, {250, 250, 250, 255}, kInk, kLine);
    draw::fillOutlined(a.seat, t, {40, 40, 46, 255}, kInk, kLine);
    draw::fillOutlined(a.plate, t, WHITE, kInk, kLine);

    // Rider: leg, torso, arm.
    const b2Vec2 hip = b2TransformPoint(t, {-0.18f, 0.50f});
    const b2Vec2 knee = b2TransformPoint(t, {0.22f, 0.50f});
    const b2Vec2 foot = b2TransformPoint(t, {0.10f, 0.04f});
    draw::line(hip, knee, 0.17f + kLine, kInk);
    draw::line(knee, foot, 0.15f + kLine, kInk);
    draw::line(hip, knee, 0.17f, {50, 60, 92, 255});
    draw::line(knee, foot, 0.15f, {50, 60, 92, 255});
    draw::circleOutlined(foot, 0.08f, {30, 30, 34, 255}, kInk, kLine);
    draw::fillOutlined(a.torso, t, {226, 52, 60, 255}, kInk, kLine);
    // Handlebar.
    const b2Vec2 bar0 = b2TransformPoint(t, {0.55f, 0.38f});
    const b2Vec2 bar1 = b2TransformPoint(t, {0.62f, 0.74f});
    draw::line(bar0, bar1, 0.07f, kInk);
    draw::line(bar1, b2TransformPoint(t, {0.76f, 0.76f}), 0.08f, kInk);
    const b2Vec2 shoulder = b2TransformPoint(t, {0.14f, 0.88f});
    const b2Vec2 elbow = b2TransformPoint(t, {0.42f, 0.70f});
    draw::line(shoulder, elbow, 0.12f + kLine, kInk);
    draw::line(elbow, bar1, 0.11f + kLine, kInk);
    draw::line(shoulder, elbow, 0.12f, {226, 52, 60, 255});
    draw::line(elbow, bar1, 0.11f, {40, 40, 46, 255});
    (void)def;
}

struct TractorArt {
    Shape hood, fender, grille, seat, torso;
};

const TractorArt& tractorArt()
{
    static const TractorArt art = [] {
        TractorArt a;
        a.hood = Shape(std::vector<b2Vec2>{{-0.30f, -0.14f}, {1.22f, -0.14f}, {1.32f, 0.04f}, {1.30f, 0.40f},
                                           {1.16f, 0.50f}, {-0.30f, 0.50f}});
        a.fender = Shape(ring({-0.80f, -0.32f}, 0.80f, 0.96f, 0.05f, kPi - 0.05f, 20));
        a.grille = Shape(roundedRect(1.12f, -0.04f, 1.30f, 0.36f, 0.04f));
        a.seat = Shape(roundedRect(-0.78f, 0.46f, -0.40f, 0.62f, 0.06f));
        a.torso = Shape(std::vector<b2Vec2>{{-0.66f, 0.56f}, {-0.30f, 0.56f}, {-0.30f, 0.86f}, {-0.42f, 1.04f},
                                            {-0.60f, 1.00f}, {-0.70f, 0.82f}});
        return a;
    }();
    return art;
}

void drawTractor(const VehicleDef& def, const VehiclePose& pose)
{
    const TractorArt& a = tractorArt();
    const b2Transform t = pose.chassis;
    const Color green{62, 160, 58, 255};

    // Exhaust stack.
    draw::line(b2TransformPoint(t, {0.78f, 0.45f}), b2TransformPoint(t, {0.78f, 1.08f}), 0.10f + kLine, kInk);
    draw::line(b2TransformPoint(t, {0.78f, 0.45f}), b2TransformPoint(t, {0.78f, 1.08f}), 0.10f, {80, 80, 90, 255});
    draw::line(b2TransformPoint(t, {0.72f, 1.10f}), b2TransformPoint(t, {0.86f, 1.10f}), 0.06f, kInk);
    // Roll frame.
    draw::line(b2TransformPoint(t, {-1.00f, 0.40f}), b2TransformPoint(t, {-0.94f, 1.26f}), 0.10f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-0.94f, 1.26f}), b2TransformPoint(t, {-0.70f, 1.28f}), 0.10f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-1.00f, 0.40f}), b2TransformPoint(t, {-0.94f, 1.26f}), 0.10f, {240, 190, 40, 255});
    draw::line(b2TransformPoint(t, {-0.94f, 1.26f}), b2TransformPoint(t, {-0.70f, 1.28f}), 0.10f, {240, 190, 40, 255});

    draw::fillOutlined(a.seat, t, {50, 46, 52, 255}, kInk, kLine);
    draw::fillOutlined(a.torso, t, {70, 110, 190, 255}, kInk, kLine);
    // Steering.
    draw::line(b2TransformPoint(t, {-0.05f, 0.46f}), b2TransformPoint(t, {-0.22f, 0.84f}), 0.06f, kInk);
    draw::line(b2TransformPoint(t, {-0.32f, 0.86f}), b2TransformPoint(t, {-0.12f, 0.82f}), 0.07f, kInk);
    const b2Vec2 shoulder = b2TransformPoint(t, {-0.46f, 0.94f});
    const b2Vec2 hand = b2TransformPoint(t, {-0.22f, 0.84f});
    draw::line(shoulder, hand, 0.12f + kLine, kInk);
    draw::line(shoulder, hand, 0.12f, kSkin);

    draw::fillOutlined(a.hood, t, green, kInk, kLine);
    draw::line(b2TransformPoint(t, {-0.25f, 0.40f}), b2TransformPoint(t, {1.15f, 0.40f}), 0.06f, {120, 200, 100, 255});
    draw::fillOutlined(a.grille, t, {60, 60, 66, 255}, kInk, kLine * 0.8f);
    for (float y = 0.02f; y < 0.34f; y += 0.08f)
        draw::line(b2TransformPoint(t, {1.15f, y}), b2TransformPoint(t, {1.27f, y}), 0.025f, {120, 120, 130, 255});
    draw::circleOutlined(b2TransformPoint(t, {1.28f, 0.44f}), 0.06f, {255, 236, 140, 255}, kInk, kLine * 0.8f);
    draw::fillOutlined(a.fender, t, {240, 190, 40, 255}, kInk, kLine);
    (void)def;
}

struct MonsterArt {
    Shape body, cab, window, flame, bumper;
};

const MonsterArt& monsterArt()
{
    static const MonsterArt art = [] {
        MonsterArt a;
        a.body = Shape(std::vector<b2Vec2>{{-1.58f, -0.06f}, {1.48f, -0.06f}, {1.66f, 0.10f}, {1.62f, 0.50f},
                                           {0.30f, 0.62f}, {-0.70f, 0.62f}, {-0.72f, 0.52f}, {-1.58f, 0.52f}});
        a.cab = Shape(std::vector<b2Vec2>{{-0.72f, 0.60f}, {0.34f, 0.60f}, {0.10f, 0.78f}, {-0.70f, 0.78f}});
        a.window = Shape(std::vector<b2Vec2>{{-0.40f, 0.62f}, {0.24f, 0.62f}, {0.08f, 0.76f}, {-0.40f, 0.76f}});
        a.flame = Shape(std::vector<b2Vec2>{{1.40f, 0.16f}, {0.70f, 0.40f}, {0.45f, 0.24f}, {0.10f, 0.42f},
                                            {-0.10f, 0.22f}, {-0.50f, 0.36f}, {-0.30f, 0.12f}, {0.60f, 0.06f}});
        a.bumper = Shape(roundedRect(1.48f, -0.14f, 1.78f, 0.12f, 0.06f));
        return a;
    }();
    return art;
}

void drawMonster(const VehicleDef& def, const VehiclePose& pose)
{
    const MonsterArt& a = monsterArt();
    const b2Transform t = pose.chassis;
    const Color blue{48, 112, 214, 255};

    // Long-travel suspension.
    for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
        const b2Vec2 mount = b2TransformPoint(t, {def.wheels[i].anchor.x, 0.05f});
        drawStrut(mount, pose.wheels[i].p, 0.13f, {200, 50, 50, 255}, true);
    }
    // Roll cage.
    const b2Vec2 c0 = b2TransformPoint(t, {-0.80f, 0.55f});
    const b2Vec2 c1 = b2TransformPoint(t, {-0.72f, 1.24f});
    const b2Vec2 c2 = b2TransformPoint(t, {0.12f, 1.22f});
    const b2Vec2 c3 = b2TransformPoint(t, {0.34f, 0.62f});
    for (auto [p, q] : {std::pair{c0, c1}, std::pair{c1, c2}, std::pair{c2, c3}}) {
        draw::line(p, q, 0.10f + kLine, kInk);
        draw::line(p, q, 0.10f, kSteel);
    }
    // Driver torso.
    const Shape torso(std::vector<b2Vec2>{{-0.42f, 0.50f}, {-0.02f, 0.50f}, {-0.04f, 0.84f}, {-0.14f, 0.98f},
                                          {-0.32f, 1.00f}, {-0.42f, 0.84f}});
    draw::fillOutlined(torso, t, {250, 200, 50, 255}, kInk, kLine);
    draw::line(b2TransformPoint(t, {-0.20f, 0.86f}), b2TransformPoint(t, {0.12f, 0.70f}), 0.12f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-0.20f, 0.86f}), b2TransformPoint(t, {0.12f, 0.70f}), 0.12f, kSkin);

    draw::fillOutlined(a.body, t, blue, kInk, kLine);
    draw::fillOutlined(a.cab, t, blue, kInk, kLine);
    draw::fill(a.flame, t, {255, 150, 40, 255});
    draw::outline(a.flame, t, 0.03f, {230, 70, 30, 255});
    draw::fillOutlined(a.window, t, {170, 220, 250, 170}, kInk, kLine * 0.8f);
    draw::fillOutlined(a.bumper, t, {200, 204, 214, 255}, kInk, kLine);
    draw::circleOutlined(b2TransformPoint(t, {1.62f, 0.34f}), 0.08f, {255, 236, 140, 255}, kInk, kLine * 0.8f);
}

struct RaceArt {
    Shape body, wing, frontWing, pod, cockpit;
};

const RaceArt& raceArt()
{
    static const RaceArt art = [] {
        RaceArt a;
        a.body = Shape(std::vector<b2Vec2>{{-1.78f, -0.12f}, {1.70f, -0.12f}, {2.02f, -0.06f}, {1.98f, 0.02f},
                                           {1.10f, 0.14f}, {0.30f, 0.24f}, {0.10f, 0.30f}, {-0.55f, 0.30f},
                                           {-0.80f, 0.44f}, {-1.30f, 0.34f}, {-1.78f, 0.24f}});
        a.wing = Shape(roundedRect(-1.98f, 0.56f, -1.36f, 0.68f, 0.05f));
        a.frontWing = Shape(roundedRect(1.62f, -0.20f, 2.10f, -0.10f, 0.04f));
        a.pod = Shape(roundedRect(-0.90f, -0.06f, 0.40f, 0.16f, 0.08f));
        a.cockpit = Shape(std::vector<b2Vec2>{{-0.52f, 0.28f}, {0.10f, 0.28f}, {-0.02f, 0.38f}, {-0.48f, 0.40f}});
        return a;
    }();
    return art;
}

void drawRaceCar(const VehicleDef& def, const VehiclePose& pose)
{
    const RaceArt& a = raceArt();
    const b2Transform t = pose.chassis;
    const Color red{232, 44, 40, 255};

    // Rear wing pillar and wing.
    draw::line(b2TransformPoint(t, {-1.62f, 0.24f}), b2TransformPoint(t, {-1.62f, 0.60f}), 0.08f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-1.62f, 0.24f}), b2TransformPoint(t, {-1.62f, 0.60f}), 0.08f, {40, 40, 46, 255});
    draw::fillOutlined(a.wing, t, red, kInk, kLine);
    draw::fillOutlined(a.body, t, red, kInk, kLine);
    draw::fillOutlined(a.pod, t, WHITE, kInk, kLine * 0.8f);
    draw::line(b2TransformPoint(t, {-0.80f, 0.05f}), b2TransformPoint(t, {0.30f, 0.05f}), 0.05f, red);
    draw::fillOutlined(a.cockpit, t, {30, 30, 36, 255}, kInk, kLine * 0.8f);
    draw::fillOutlined(a.frontWing, t, {40, 40, 46, 255}, kInk, kLine);
    // Number roundel.
    draw::circleOutlined(b2TransformPoint(t, {1.10f, 0.04f}), 0.10f, WHITE, kInk, kLine * 0.7f);
    draw::line(b2TransformPoint(t, {1.07f, 0.10f}), b2TransformPoint(t, {1.13f, 0.10f}), 0.025f, kInk);
    draw::line(b2TransformPoint(t, {1.13f, 0.10f}), b2TransformPoint(t, {1.08f, -0.02f}), 0.025f, kInk);
    (void)def;
}

b2Vec2 safeNorm(b2Vec2 v, b2Vec2 fallback)
{
    const float len = b2Length(v);
    return len > 1e-5f ? b2MulSV(1.0f / len, v) : fallback;
}

struct MoonlanderArt {
    Shape base, cabin, panel, goldLeft, goldRight, nozzleL, nozzleR, canopy, collar;
};

const MoonlanderArt& moonlanderArt()
{
    static const MoonlanderArt art = [] {
        MoonlanderArt a;
        a.base = Shape(std::vector<b2Vec2>{{-1.15f, -0.18f}, {1.15f, -0.18f}, {1.02f, 0.02f}, {-1.02f, 0.02f}});
        a.cabin = Shape(std::vector<b2Vec2>{{-0.78f, 0.02f}, {0.78f, 0.02f}, {0.92f, 0.28f}, {0.58f, 0.58f},
                                            {-0.58f, 0.58f}, {-0.92f, 0.28f}});
        a.panel = Shape(roundedRect(-0.52f, 0.14f, 0.52f, 0.44f, 0.04f));
        a.goldLeft = Shape(std::vector<b2Vec2>{{-0.74f, 0.06f}, {-0.56f, 0.06f}, {-0.56f, 0.48f}, {-0.84f, 0.28f}});
        a.goldRight = Shape(std::vector<b2Vec2>{{0.56f, 0.06f}, {0.74f, 0.06f}, {0.84f, 0.28f}, {0.56f, 0.48f}});
        a.nozzleL = Shape(std::vector<b2Vec2>{{-0.72f, -0.37f}, {-0.40f, -0.37f}, {-0.47f, -0.18f}, {-0.65f, -0.18f}});
        a.nozzleR = Shape(std::vector<b2Vec2>{{0.40f, -0.37f}, {0.72f, -0.37f}, {0.65f, -0.18f}, {0.47f, -0.18f}});
        std::vector<b2Vec2> dome;
        for (int i = 0; i <= 18; ++i) {
            const float ang = kPi * static_cast<float>(i) / 18.0f;
            dome.push_back({0.55f * std::cos(ang), 0.57f + 0.62f * std::sin(ang)});
        }
        a.canopy = Shape(dome);
        a.collar = Shape(roundedRect(-0.30f, 0.52f, 0.30f, 0.68f, 0.07f));
        return a;
    }();
    return art;
}

void drawMoonlander(const VehicleDef& def, const VehiclePose& pose, float time, const WheelStyle& wheelStyle)
{
    const MoonlanderArt& a = moonlanderArt();
    const b2Transform t = pose.chassis;
    const Color gold{234, 180, 44, 255};
    const Color goldDark{184, 132, 24, 255};
    const Color whiteBody{240, 242, 248, 255};

    // Telescopic landing gear struts + diagonal A-arms to all 3 live wheels.
    for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
        const float ax = def.wheels[i].anchor.x;
        const b2Vec2 hub = pose.wheels[i].p;
        if (i == 1) {
            drawStrut(b2TransformPoint(t, {-0.26f, -0.12f}), hub, 0.055f, goldDark, false);
            drawStrut(b2TransformPoint(t, {0.26f, -0.12f}), hub, 0.055f, goldDark, false);
            drawStrut(b2TransformPoint(t, {0.0f, -0.02f}), hub, 0.075f, kSteel, true);
        } else {
            drawStrut(b2TransformPoint(t, {ax * 0.55f, -0.14f}), hub, 0.055f, goldDark, false);
            drawStrut(b2TransformPoint(t, {ax * 0.84f, -0.02f}), hub, 0.075f, kSteel, true);
        }
    }

    // Lunar wire-mesh wheels.
    for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
        drawWheel(pose.wheels[i], def.wheels[i].radius, wheelStyle);
    }

    // Rocket thrust flames out of twin bottom thruster nozzles.
    if (pose.thrusting) {
        for (float nx : {-0.56f, 0.56f}) {
            const float flick = 0.88f + 0.22f * std::sin(time * 48.0f + nx * 6.0f) + 0.10f * std::cos(time * 83.0f);
            const b2Vec2 noz = b2TransformPoint(t, {nx, -0.37f});
            draw::circle(b2TransformPoint(t, {nx, -0.44f}), 0.24f * flick, draw::withAlpha({255, 190, 60, 255}, 0.35f));
            auto cone = [&](float halfW, float len, Color col) {
                const Vector2 pL = draw::rv(b2TransformPoint(t, {nx - halfW, -0.36f}));
                const Vector2 pR = draw::rv(b2TransformPoint(t, {nx + halfW, -0.36f}));
                const Vector2 pTip = draw::rv(b2TransformPoint(t, {nx, -0.36f - len * flick}));
                DrawTriangle(pL, pTip, pR, col);
            };
            cone(0.15f, 1.05f, {255, 112, 24, 235});
            cone(0.10f, 0.76f, {255, 222, 62, 245});
            cone(0.055f, 0.44f, {225, 248, 255, 255});
            (void)noz;
        }
    }

    // Twin bottom thruster bells & gold-foil lower propulsion base.
    draw::fillOutlined(a.nozzleL, t, {76, 80, 92, 255}, kInk, kLine);
    draw::fillOutlined(a.nozzleR, t, {76, 80, 92, 255}, kInk, kLine);
    for (float nx : {-0.56f, 0.56f}) {
        draw::line(b2TransformPoint(t, {nx - 0.12f, -0.27f}), b2TransformPoint(t, {nx + 0.12f, -0.27f}), 0.03f, kSteel);
    }
    draw::fillOutlined(a.base, t, gold, kInk, kLine);
    for (float x = -0.82f; x <= 0.85f; x += 0.41f) {
        draw::line(b2TransformPoint(t, {x, -0.16f}), b2TransformPoint(t, {x * 0.90f, 0.00f}), 0.03f, goldDark);
    }

    // Rear dish antenna.
    const b2Vec2 antBase = b2TransformPoint(t, {-0.72f, 0.46f});
    const b2Vec2 antTip = b2TransformPoint(t, {-0.98f, 0.96f});
    draw::line(antBase, antTip, 0.05f + kLine, kInk);
    draw::line(antBase, antTip, 0.05f, kSteel);
    draw::line(b2TransformPoint(t, {-0.98f, 0.74f}), b2TransformPoint(t, {-0.80f, 0.92f}), 0.06f, kSteel);
    draw::circleOutlined(antTip, 0.055f, {235, 65, 55, 255}, kInk, kLine * 0.7f);

    // Hexagonal lunar cabin + NASA-style black & gold panels.
    draw::fillOutlined(a.cabin, t, whiteBody, kInk, kLine);
    draw::fillOutlined(a.goldLeft, t, gold, kInk, kLine * 0.8f);
    draw::fillOutlined(a.goldRight, t, gold, kInk, kLine * 0.8f);
    draw::fillOutlined(a.panel, t, {36, 42, 54, 255}, kInk, kLine * 0.8f);
    draw::line(b2TransformPoint(t, {-0.40f, 0.34f}), b2TransformPoint(t, {0.40f, 0.34f}), 0.045f, gold);
    draw::line(b2TransformPoint(t, {-0.40f, 0.24f}), b2TransformPoint(t, {0.20f, 0.24f}), 0.035f, {72, 136, 224, 255});
    // RCS pods on cabin sides.
    for (float sx : {-0.92f, 0.92f}) {
        draw::circleOutlined(b2TransformPoint(t, {sx, 0.28f}), 0.085f, kSteelDark, kInk, kLine * 0.8f);
    }

    // Astronaut torso collar + head (drawn inside the canopy!).
    draw::fillOutlined(a.collar, t, whiteBody, kInk, kLine);
    drawHead(pose.head, def.headRadius, Headgear::FullFace, {246, 248, 252, 255}, {236, 178, 42, 255}, pose.crashed,
             true);

    // Translucent cyan glass canopy dome + specular glint + curved frame struts over/around it.
    draw::fill(a.canopy, t, draw::withAlpha({140, 230, 255, 255}, 0.28f));
    for (int i = 0; i < 6; ++i) {
        const float a0 = 0.28f + 0.16f * static_cast<float>(i);
        const float a1 = a0 + 0.16f;
        draw::line(b2TransformPoint(t, {0.46f * std::cos(a0), 0.59f + 0.53f * std::sin(a0)}),
                   b2TransformPoint(t, {0.46f * std::cos(a1), 0.59f + 0.53f * std::sin(a1)}), 0.035f,
                   draw::withAlpha({225, 250, 255, 255}, 0.65f));
    }
    // Curved canopy frame struts.
    draw::outline(a.canopy, t, 0.065f, kInk);
    for (size_t i = 0; i + 1 < a.canopy.points.size(); ++i) {
        draw::line(b2TransformPoint(t, a.canopy.points[i]), b2TransformPoint(t, a.canopy.points[i + 1]), 0.035f, kSteel);
    }
    for (int i = 0; i < 8; ++i) {
        const float u0 = static_cast<float>(i) / 8.0f;
        const float u1 = static_cast<float>(i + 1) / 8.0f;
        auto ribPt = [](float u) -> b2Vec2 {
            const float ang = lerpf(0.0f, kPi * 0.62f, u);
            return {0.10f + 0.26f * std::cos(ang) - 0.18f * u, 0.57f + 0.58f * std::sin(ang * 0.85f)};
        };
        const b2Vec2 p0 = b2TransformPoint(t, ribPt(u0));
        const b2Vec2 p1 = b2TransformPoint(t, ribPt(u1));
        draw::line(p0, p1, 0.055f, kInk);
        draw::line(p0, p1, 0.028f, kSteel);
    }
    draw::line(b2TransformPoint(t, {-0.58f, 0.57f}), b2TransformPoint(t, {0.58f, 0.57f}), 0.075f, kInk);
    draw::line(b2TransformPoint(t, {-0.58f, 0.57f}), b2TransformPoint(t, {0.58f, 0.57f}), 0.045f, kSteel);
}

struct RallyArt {
    Shape body, scoop, spoiler, mudRear, mudFront, seat, torso, quarterGlass, goldSwoosh;
};

const RallyArt& rallyArt()
{
    static const RallyArt art = [] {
        RallyArt a;
        std::vector<b2Vec2> body{{-1.62f, -0.16f}};
        appendArch(body, {-1.12f, -0.34f}, 0.50f, -0.16f);
        appendArch(body, {1.14f, -0.34f}, 0.50f, -0.16f);
        for (b2Vec2 p : {b2Vec2{1.63f, -0.16f}, b2Vec2{1.67f, 0.04f}, b2Vec2{1.54f, 0.22f}, b2Vec2{1.38f, 0.36f},
                         b2Vec2{0.38f, 0.38f}, b2Vec2{0.26f, 0.24f}, b2Vec2{-0.96f, 0.24f}, b2Vec2{-0.82f, 0.88f},
                         b2Vec2{-1.16f, 0.88f}, b2Vec2{-1.56f, 0.22f}, b2Vec2{-1.64f, 0.08f}})
            body.push_back(p);
        a.body = Shape(body);
        a.scoop = Shape(std::vector<b2Vec2>{{0.62f, 0.36f}, {1.08f, 0.35f}, {1.02f, 0.47f}, {0.72f, 0.46f}});
        a.spoiler = Shape(std::vector<b2Vec2>{{-1.66f, 1.00f}, {-1.10f, 0.94f}, {-1.14f, 1.07f}, {-1.70f, 1.13f}});
        a.mudRear = Shape(std::vector<b2Vec2>{{-1.70f, -0.46f}, {-1.56f, -0.46f}, {-1.48f, -0.14f}, {-1.62f, -0.14f}});
        a.mudFront = Shape(std::vector<b2Vec2>{{0.52f, -0.44f}, {0.65f, -0.44f}, {0.73f, -0.14f}, {0.60f, -0.14f}});
        a.seat = Shape(roundedRect(-0.56f, 0.20f, -0.34f, 0.68f, 0.06f));
        a.torso = Shape(std::vector<b2Vec2>{{-0.38f, 0.20f}, {-0.02f, 0.20f}, {-0.04f, 0.44f}, {-0.14f, 0.56f},
                                            {-0.30f, 0.56f}, {-0.38f, 0.42f}});
        a.quarterGlass = Shape(std::vector<b2Vec2>{{-1.36f, 0.28f}, {-0.98f, 0.28f}, {-0.88f, 0.80f}, {-1.12f, 0.80f}});
        a.goldSwoosh = Shape(std::vector<b2Vec2>{{-0.55f, -0.06f}, {0.58f, 0.08f}, {0.50f, 0.19f}, {-0.62f, 0.08f}});
        return a;
    }();
    return art;
}

void drawRally(const VehicleDef& def, const VehiclePose& pose)
{
    const RallyArt& a = rallyArt();
    const b2Transform t = pose.chassis;
    const Color wrcBlue{32, 92, 210, 255};
    const Color wrcGold{250, 204, 48, 255};
    const Color mudRed{218, 42, 42, 255};

    // Exhaust tailpipe & red mudflaps behind wheels.
    draw::line(b2TransformPoint(t, {-1.42f, -0.10f}), b2TransformPoint(t, {-1.64f, -0.10f}), 0.11f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-1.42f, -0.10f}), b2TransformPoint(t, {-1.64f, -0.10f}), 0.11f, kSteel);
    draw::fillOutlined(a.mudRear, t, mudRed, kInk, kLine * 0.8f);
    draw::fillOutlined(a.mudFront, t, mudRed, kInk, kLine * 0.8f);

    // Interior seat, driver racing suit, steering wheel.
    draw::fillOutlined(a.seat, t, {48, 46, 56, 255}, kInk, kLine);
    draw::fillOutlined(a.torso, t, {28, 76, 178, 255}, kInk, kLine);
    draw::line(b2TransformPoint(t, {0.26f, 0.26f}), b2TransformPoint(t, {0.10f, 0.50f}), 0.055f, kInk);
    draw::line(b2TransformPoint(t, {0.04f, 0.58f}), b2TransformPoint(t, {0.16f, 0.42f}), 0.065f, kInk);
    const b2Vec2 shoulder = b2TransformPoint(t, {-0.18f, 0.48f});
    const b2Vec2 hand = b2TransformPoint(t, {0.10f, 0.50f});
    draw::line(shoulder, hand, 0.11f + kLine, kInk);
    draw::line(shoulder, hand, 0.11f, {240, 242, 248, 255});

    // Rear spoiler pillar + wing.
    draw::line(b2TransformPoint(t, {-1.16f, 0.84f}), b2TransformPoint(t, {-1.36f, 1.02f}), 0.09f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-1.16f, 0.84f}), b2TransformPoint(t, {-1.36f, 1.02f}), 0.09f, wrcBlue);
    draw::fillOutlined(a.spoiler, t, wrcBlue, kInk, kLine);
    draw::line(b2TransformPoint(t, {-1.66f, 1.06f}), b2TransformPoint(t, {-1.16f, 1.01f}), 0.04f, wrcGold);

    // WRC blue & gold hatchback body.
    draw::fillOutlined(a.body, t, wrcBlue, kInk, kLine);
    draw::fill(a.goldSwoosh, t, wrcGold);
    draw::line(b2TransformPoint(t, {-1.42f, 0.16f}), b2TransformPoint(t, {0.28f, 0.18f}), 0.055f, WHITE);
    draw::line(b2TransformPoint(t, {-0.96f, 0.30f}), b2TransformPoint(t, {-1.38f, 0.68f}), 0.08f, wrcGold);
    draw::fillOutlined(a.quarterGlass, t, {44, 58, 82, 220}, kInk, kLine * 0.8f);
    // Door crease.
    draw::line(b2TransformPoint(t, {-0.46f, -0.10f}), b2TransformPoint(t, {-0.46f, 0.22f}), 0.03f, draw::shade(wrcBlue, 0.7f));
    draw::line(b2TransformPoint(t, {0.22f, -0.10f}), b2TransformPoint(t, {0.22f, 0.22f}), 0.03f, draw::shade(wrcBlue, 0.7f));
    // Hood intake scoop & front twin rally lamps.
    draw::fillOutlined(a.scoop, t, draw::shade(wrcBlue, 0.82f), kInk, kLine * 0.8f);
    draw::line(b2TransformPoint(t, {1.06f, 0.38f}), b2TransformPoint(t, {1.02f, 0.44f}), 0.045f, kInk);
    draw::circleOutlined(b2TransformPoint(t, {1.54f, 0.22f}), 0.07f, {255, 242, 160, 255}, kInk, kLine * 0.75f);
    draw::circleOutlined(b2TransformPoint(t, {1.62f, 0.10f}), 0.07f, {255, 242, 160, 255}, kInk, kLine * 0.75f);
    draw::circleOutlined(b2TransformPoint(t, {-1.54f, 0.16f}), 0.055f, {245, 58, 50, 255}, kInk, kLine * 0.75f);

    // Breakable tubular steel roll cage matching def.rollCage.
    const Color cageSteel{215, 222, 232, 255};
    if (!pose.rollCageBroken) {
        const b2Vec2 pRearBase = b2TransformPoint(t, {-0.98f, 0.26f});
        const b2Vec2 pRearTop = b2TransformPoint(t, {-0.80f, 0.99f});
        const b2Vec2 pFrontTop = b2TransformPoint(t, {0.35f, 0.99f});
        const b2Vec2 pFrontBase = b2TransformPoint(t, {0.37f, 0.36f});
        const b2Vec2 pMidRoof = b2TransformPoint(t, {-0.26f, 0.99f});

        auto cageTube = [&](b2Vec2 p0, b2Vec2 p1, float w) {
            draw::line(p0, p1, w + kLine, kInk);
            draw::line(p0, p1, w, cageSteel);
        };
        cageTube(pRearBase, pMidRoof, 0.065f);
        cageTube(pRearBase, pRearTop, 0.085f);
        cageTube(pRearTop, pFrontTop, 0.095f);
        cageTube(pFrontTop, pFrontBase, 0.085f);

        // Damage feedback: orange stress marks & dark crack notches.
        if (pose.rollCageDamage > 0) {
            const Color stress{255, 130, 32, 235};
            auto crackAt = [&](b2Vec2 pt, b2Vec2 d) {
                draw::circle(pt, 0.085f, draw::withAlpha(stress, 0.75f));
                draw::line(b2Sub(pt, d), b2Add(pt, d), 0.038f, kInk);
            };
            crackAt(b2TransformPoint(t, {-0.22f, 0.99f}), b2RotateVector(t.q, {0.06f, 0.08f}));
            crackAt(b2TransformPoint(t, {0.36f, 0.68f}), b2RotateVector(t.q, {0.07f, -0.05f}));
            if (pose.rollCageDamage >= 2) {
                crackAt(b2TransformPoint(t, {-0.58f, 0.99f}), b2RotateVector(t.q, {-0.05f, 0.08f}));
                crackAt(b2TransformPoint(t, {0.14f, 0.99f}), b2RotateVector(t.q, {0.06f, -0.07f}));
                draw::line(b2TransformPoint(t, {-0.38f, 0.96f}), b2TransformPoint(t, {0.04f, 1.02f}), 0.045f, stress);
            }
        }
    } else {
        // Broken roll cage: jagged snapped stubs where pillars used to be.
        auto stub = [&](b2Vec2 basePt, b2Vec2 tipPt, b2Vec2 jag) {
            draw::line(basePt, tipPt, 0.085f + kLine, kInk);
            draw::line(basePt, tipPt, 0.085f, cageSteel);
            draw::line(tipPt, b2Add(tipPt, jag), 0.04f, kInk);
            draw::line(tipPt, b2Sub(tipPt, jag), 0.035f, {160, 166, 178, 255});
        };
        stub(b2TransformPoint(t, {0.37f, 0.36f}), b2TransformPoint(t, {0.40f, 0.55f}), b2RotateVector(t.q, {-0.04f, 0.06f}));
        stub(b2TransformPoint(t, {-0.88f, 0.88f}), b2TransformPoint(t, {-0.68f, 0.95f}), b2RotateVector(t.q, {0.06f, 0.04f}));
        stub(b2TransformPoint(t, {-0.98f, 0.26f}), b2TransformPoint(t, {-0.92f, 0.44f}), b2RotateVector(t.q, {0.04f, 0.05f}));
    }
    (void)def;
}

struct TankArt {
    Shape hull, skirt, turret, hatch, cupola, barrel, evacuator, muzzle, torso, star;
};

const TankArt& tankArt()
{
    static const TankArt art = [] {
        TankArt a;
        a.hull = Shape(std::vector<b2Vec2>{{-1.54f, -0.16f}, {1.54f, -0.16f}, {1.64f, 0.12f}, {1.44f, 0.34f},
                                           {-1.48f, 0.34f}, {-1.58f, 0.16f}});
        a.skirt = Shape(roundedRect(-1.50f, -0.02f, 1.50f, 0.16f, 0.04f));
        a.turret = Shape(std::vector<b2Vec2>{{-0.88f, 0.34f}, {0.48f, 0.34f}, {0.52f, 0.52f}, {0.34f, 0.72f},
                                             {-0.76f, 0.72f}, {-0.92f, 0.52f}});
        a.hatch = Shape(roundedRect(-0.40f, 0.70f, 0.10f, 0.80f, 0.03f));
        a.cupola = Shape(roundedRect(-0.84f, 0.70f, -0.52f, 0.79f, 0.03f));
        a.barrel = Shape(std::vector<b2Vec2>{{0.44f, 0.48f}, {1.84f, 0.51f}, {1.84f, 0.62f}, {0.44f, 0.61f}});
        a.evacuator = Shape(roundedRect(1.00f, 0.46f, 1.32f, 0.66f, 0.03f));
        a.muzzle = Shape(roundedRect(1.80f, 0.45f, 1.98f, 0.68f, 0.03f));
        a.torso = Shape(std::vector<b2Vec2>{{-0.30f, 0.74f}, {0.00f, 0.74f}, {-0.04f, 0.90f}, {-0.26f, 0.90f}});
        std::vector<b2Vec2> starPts;
        for (int i = 0; i < 10; ++i) {
            const float ang = kPi * 0.5f + static_cast<float>(i) * kPi * 0.2f;
            const float rad = (i % 2 == 0) ? 0.12f : 0.05f;
            starPts.push_back({rad * std::cos(ang), 0.24f + rad * std::sin(ang)});
        }
        a.star = Shape(starPts);
        return a;
    }();
    return art;
}

void drawTank(const VehicleDef& def, const VehiclePose& pose, const WheelStyle& wheelStyle)
{
    const TankArt& a = tankArt();
    const b2Transform t = pose.chassis;
    const Color olive{92, 108, 66, 255};
    const Color oliveDark{68, 82, 48, 255};
    const Color oliveLight{118, 134, 88, 255};

    // Dark recessed undercarriage behind wheels & tracks.
    if (pose.wheels.size() >= 2) {
        draw::line(pose.wheels.front().p, pose.wheels.back().p, 0.34f, {26, 28, 30, 220});
    }

    // Road wheels inside the track belt.
    for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
        drawWheel(pose.wheels[i], def.wheels[i].radius, wheelStyle);
    }

    // Continuous rubber/steel track belt wrapping smoothly around all live pose.wheels.
    if (pose.wheels.size() >= 2) {
        const size_t nw = pose.wheels.size();
        const float r = def.wheels[0].radius;
        const float trackR = r + 0.032f;
        std::vector<b2Vec2> c(nw);
        for (size_t i = 0; i < nw; ++i) c[i] = pose.wheels[i].p;

        const b2Vec2 fwd = b2RotateVector(t.q, {1.0f, 0.0f});
        const b2Vec2 spanDir = safeNorm(b2Sub(c[nw - 1], c[0]), fwd);
        const b2Vec2 nTop{-spanDir.y, spanDir.x};
        const float aTop = std::atan2(nTop.y, nTop.x);

        std::vector<b2Vec2> nBot(nw - 1);
        for (size_t i = 0; i + 1 < nw; ++i) {
            const b2Vec2 sd = safeNorm(b2Sub(c[i + 1], c[i]), fwd);
            nBot[i] = {sd.y, -sd.x};
        }

        std::vector<b2Vec2> trackPts;
        trackPts.reserve(64);

        // 1. Top tangent run from rear wheel 0 to front wheel last + subtle upper return curve.
        constexpr int kTopSteps = 6;
        const b2Vec2 topStart = b2MulAdd(c[0], trackR, nTop);
        const b2Vec2 topEnd = b2MulAdd(c[nw - 1], trackR, nTop);
        for (int s = 0; s <= kTopSteps; ++s) {
            const float u = static_cast<float>(s) / static_cast<float>(kTopSteps);
            b2Vec2 pt = b2MulAdd(b2Lerp(topStart, topEnd, u), 0.045f * std::sin(kPi * u), nTop);
            for (size_t w = 1; w + 1 < nw; ++w) {
                const b2Vec2 d = b2Sub(pt, c[w]);
                if (std::fabs(b2Dot(d, spanDir)) < trackR) {
                    const float proj = b2Dot(d, nTop);
                    if (proj < trackR) pt = b2MulAdd(pt, trackR - proj, nTop);
                }
            }
            trackPts.push_back(pt);
        }

        // 2. Clockwise wrap around front wheel.
        const float aFrontBot = std::atan2(nBot[nw - 2].y, nBot[nw - 2].x);
        float daFront = wrapAngle(aFrontBot - aTop);
        if (daFront > 0.0f) daFront -= kTwoPi;
        for (int s = 1; s <= 10; ++s) {
            const float ang = aTop + daFront * (static_cast<float>(s) / 10.0f);
            trackPts.push_back({c[nw - 1].x + trackR * std::cos(ang), c[nw - 1].y + trackR * std::sin(ang)});
        }

        // 3. Bottom run connecting through the bottom of each live wheel.
        for (size_t i = nw - 2; i >= 1; --i) {
            const float aIn = std::atan2(nBot[i].y, nBot[i].x);
            const float aOut = std::atan2(nBot[i - 1].y, nBot[i - 1].x);
            const float da = wrapAngle(aOut - aIn);
            if (da < -0.05f) {
                for (int s = 0; s <= 3; ++s) {
                    const float ang = aIn + da * (static_cast<float>(s) / 3.0f);
                    trackPts.push_back({c[i].x + trackR * std::cos(ang), c[i].y + trackR * std::sin(ang)});
                }
            } else {
                const b2Vec2 bis = safeNorm(b2Add(nBot[i], nBot[i - 1]), nBot[i]);
                trackPts.push_back(b2MulAdd(c[i], trackR, bis));
            }
        }

        // 4. Clockwise wrap around rear wheel.
        const float aRearBot = std::atan2(nBot[0].y, nBot[0].x);
        float daRear = wrapAngle(aTop - aRearBot);
        if (daRear > 0.0f) daRear -= kTwoPi;
        for (int s = 0; s < 10; ++s) {
            const float ang = aRearBot + daRear * (static_cast<float>(s) / 10.0f);
            trackPts.push_back({c[0].x + trackR * std::cos(ang), c[0].y + trackR * std::sin(ang)});
        }

        // Draw track loop + moving cleats driven by wheel 0 angle.
        const size_t np = trackPts.size();
        for (size_t i = 0; i < np; ++i) {
            draw::line(trackPts[i], trackPts[(i + 1) % np], 0.11f, kInk);
        }
        for (size_t i = 0; i < np; ++i) {
            draw::line(trackPts[i], trackPts[(i + 1) % np], 0.065f, {48, 50, 56, 255});
        }

        std::vector<float> cumLen(np + 1, 0.0f);
        for (size_t i = 0; i < np; ++i) {
            cumLen[i + 1] = cumLen[i] + b2Distance(trackPts[i], trackPts[(i + 1) % np]);
        }
        const float totalLen = cumLen.back();
        if (totalLen > 0.1f) {
            constexpr int kCleatsPerRev = 6;
            const int numCleats = std::max(12, static_cast<int>(std::round(totalLen / (kTwoPi * r / static_cast<float>(kCleatsPerRev)))));
            const float cleatSpacing = totalLen / static_cast<float>(numCleats);
            const float wheelAngle = b2Rot_GetAngle(pose.wheels[0].q);
            const float offset = (-wheelAngle / kTwoPi) * static_cast<float>(kCleatsPerRev) * cleatSpacing;

            size_t seg = 0;
            for (int m = 0; m < numCleats; ++m) {
                float sPos = std::fmod(static_cast<float>(m) * cleatSpacing + offset, totalLen);
                if (sPos < 0.0f) sPos += totalLen;
                if (sPos < cumLen[seg]) seg = 0;
                while (seg + 1 < np && cumLen[seg + 1] < sPos) ++seg;
                const float segLen = cumLen[seg + 1] - cumLen[seg];
                const float frac = segLen > 1e-5f ? (sPos - cumLen[seg]) / segLen : 0.0f;
                const b2Vec2 pA = trackPts[seg];
                const b2Vec2 pB = trackPts[(seg + 1) % np];
                const b2Vec2 pos = b2Lerp(pA, pB, frac);
                const b2Vec2 dir = safeNorm(b2Sub(pB, pA), fwd);
                const b2Vec2 nOut{-dir.y, dir.x};
                draw::line(pos, b2MulAdd(pos, 0.068f, nOut), 0.064f, kInk);
                draw::line(pos, b2MulAdd(pos, 0.058f, nOut), 0.038f, {92, 96, 106, 255});
            }
        }
    }

    // Rear exhaust muffler + long cannon barrel (behind turret edge).
    draw::line(b2TransformPoint(t, {-1.40f, 0.16f}), b2TransformPoint(t, {-1.58f, 0.16f}), 0.11f + kLine, kInk);
    draw::line(b2TransformPoint(t, {-1.40f, 0.16f}), b2TransformPoint(t, {-1.58f, 0.16f}), 0.11f, {64, 68, 70, 255});

    draw::fillOutlined(a.barrel, t, oliveDark, kInk, kLine);
    draw::fillOutlined(a.evacuator, t, olive, kInk, kLine);
    draw::fillOutlined(a.muzzle, t, oliveDark, kInk, kLine);
    draw::line(b2TransformPoint(t, {1.86f, 0.56f}), b2TransformPoint(t, {1.94f, 0.56f}), 0.05f, kInk);

    // Armored sloped hull, side skirts overlapping top track run, rivets & white star.
    draw::fillOutlined(a.hull, t, olive, kInk, kLine);
    draw::line(b2TransformPoint(t, {-1.38f, 0.28f}), b2TransformPoint(t, {1.38f, 0.28f}), 0.05f, oliveLight);
    draw::fillOutlined(a.skirt, t, oliveDark, kInk, kLine);
    for (float rx = -1.36f; rx <= 1.38f; rx += 0.34f) {
        draw::circle(b2TransformPoint(t, {rx, 0.07f}), 0.028f, oliveLight);
    }
    draw::fill(a.star, t, WHITE);

    // Rounded turret, commander cupola & torso.
    draw::fillOutlined(a.torso, t, {76, 88, 56, 255}, kInk, kLine);
    draw::fillOutlined(a.turret, t, olive, kInk, kLine);
    draw::fillOutlined(a.hatch, t, oliveLight, kInk, kLine * 0.85f);
    draw::fillOutlined(a.cupola, t, oliveDark, kInk, kLine * 0.85f);
    draw::circleOutlined(b2TransformPoint(t, {0.46f, 0.52f}), 0.10f, oliveDark, kInk, kLine * 0.8f);
    draw::circle(b2TransformPoint(t, {-0.54f, 0.54f}), 0.032f, oliveLight);
    draw::circle(b2TransformPoint(t, {0.18f, 0.54f}), 0.032f, oliveLight);
}

WheelStyle wheelStyleFor(const std::string& id)
{
    WheelStyle s;
    if (id == "bike") {
        s.knobs = 18;
        s.rimScale = 0.70f;
        s.spokes = 12;
        s.rim = {220, 222, 230, 255};
    } else if (id == "tractor") {
        s.knobs = 12;
        s.knobDepth = 0.12f;
        s.rim = {246, 196, 40, 255};
        s.hub = {200, 150, 20, 255};
        s.spokes = 6;
    } else if (id == "monster") {
        s.knobs = 16;
        s.knobDepth = 0.10f;
        s.rimScale = 0.50f;
        s.rim = {80, 84, 96, 255};
        s.hub = {200, 204, 214, 255};
        s.spokes = 6;
    } else if (id == "racecar") {
        s.slick = true;
        s.rimScale = 0.62f;
        s.rim = {250, 210, 60, 255};
        s.hub = {40, 40, 46, 255};
        s.spokes = 6;
    } else if (id == "moonlander") {
        s.knobs = 16;
        s.knobDepth = 0.07f;
        s.rimScale = 0.68f;
        s.tire = {126, 134, 148, 255};
        s.rim = {222, 228, 238, 255};
        s.hub = {234, 180, 44, 255};
        s.spokes = 10;
        s.wireMesh = true;
    } else if (id == "rally") {
        s.knobs = 16;
        s.knobDepth = 0.085f;
        s.rimScale = 0.60f;
        s.tire = {40, 40, 46, 255};
        s.rim = {250, 204, 48, 255};
        s.hub = {32, 92, 210, 255};
        s.spokes = 8;
    } else if (id == "tank") {
        s.slick = true;
        s.rimScale = 0.66f;
        s.tire = {46, 50, 54, 255};
        s.rim = {86, 94, 82, 255};
        s.hub = {92, 108, 66, 255};
        s.spokes = 6;
    }
    return s;
}

} // namespace

void drawVehicle(const VehicleDef& def, const VehiclePose& pose, float time)
{
    const WheelStyle wheelStyle = wheelStyleFor(def.id);

    // Struts for the vehicles that show them, then wheels behind the bike frame.
    if (def.id == "jeep" || def.id == "racecar" || def.id == "tractor" || def.id == "rally") {
        for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
            drawStrut(b2TransformPoint(pose.chassis, {def.wheels[i].anchor.x, def.wheels[i].anchor.y + 0.30f}),
                      pose.wheels[i].p, 0.07f, kSteelDark, def.id == "jeep" || def.id == "rally");
        }
    }

    // Vibrant electric-cyan/white flickering nitro flame cone shooting from the exhaust.
    if (pose.boosting && !def.hasThrusters()) {
        const b2Vec2 origin = b2TransformPoint(pose.chassis, exhaustPoint(def));
        const b2Vec2 dir = safeNorm(b2RotateVector(pose.chassis.q, exhaustDirection(def)), {-1.0f, 0.0f});
        const b2Vec2 perp{-dir.y, dir.x};
        const float flick = 0.85f + 0.25f * std::sin(time * 58.0f) + 0.12f * std::cos(time * 93.0f);
        auto drawCone = [&](float halfW, float len, Color c) {
            const Vector2 pL = draw::rv(b2MulAdd(origin, halfW, perp));
            const Vector2 pR = draw::rv(b2MulAdd(origin, -halfW, perp));
            const Vector2 pTip = draw::rv(b2MulAdd(origin, len * flick, dir));
            DrawTriangle(pR, pTip, pL, c);
        };
        draw::circle(b2MulAdd(origin, 0.10f, dir), 0.19f, draw::withAlpha({40, 210, 255, 255}, 0.35f));
        drawCone(0.16f, 1.15f, {30, 185, 255, 230});
        drawCone(0.11f, 0.82f, {125, 245, 255, 245});
        drawCone(0.06f, 0.48f, {245, 252, 255, 255});
    }

    const bool customWheels = def.id == "moonlander" || def.id == "tank";
    const bool wheelsBehind = def.id == "bike";
    auto drawWheels = [&] {
        for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i)
            drawWheel(pose.wheels[i], def.wheels[i].radius, wheelStyle);
    };
    if (wheelsBehind) drawWheels();

    Headgear gear = Headgear::Helmet;
    Color gearColor{250, 204, 52, 255};
    Color accent{220, 60, 50, 255};
    Color accent2 = BLANK;
    if (def.id == "jeep") drawJeep(def, pose);
    else if (def.id == "bike") {
        drawBike(def, pose);
        gear = Headgear::FullFace;
        gearColor = {245, 245, 250, 255};
        accent = {226, 52, 60, 255};
    } else if (def.id == "tractor") {
        drawTractor(def, pose);
        gear = Headgear::StrawHat;
        gearColor = {236, 200, 110, 255};
        accent = {200, 60, 50, 255};
    } else if (def.id == "monster") {
        drawMonster(def, pose);
        gearColor = {226, 52, 60, 255};
        accent = {255, 255, 255, 255};
    } else if (def.id == "racecar") {
        drawRaceCar(def, pose);
        gear = Headgear::RaceHelmet;
        gearColor = {250, 220, 40, 255};
        accent = {40, 120, 230, 255};
    } else if (def.id == "moonlander") {
        drawMoonlander(def, pose, time, wheelStyle);
    } else if (def.id == "rally") {
        drawRally(def, pose);
        gear = Headgear::RaceHelmet;
        gearColor = {246, 248, 252, 255};
        accent = {32, 92, 210, 255};
        accent2 = {250, 204, 48, 255};
    } else if (def.id == "tank") {
        drawTank(def, pose, wheelStyle);
        gear = Headgear::Helmet;
        gearColor = {84, 98, 60, 255};
        accent = {210, 174, 72, 255};
    }

    if (!wheelsBehind && !customWheels) drawWheels();
    if (def.id != "moonlander") {
        drawHead(pose.head, def.headRadius, gear, gearColor, accent, pose.crashed, false, accent2);
    }
}

b2Vec2 exhaustPoint(const VehicleDef& def)
{
    if (def.id == "bike") return {-0.88f, 0.16f};
    if (def.id == "tractor") return {0.78f, 1.12f};
    if (def.id == "monster") return {-1.60f, 0.10f};
    if (def.id == "racecar") return {-1.80f, 0.10f};
    if (def.id == "moonlander") return {-0.56f, -0.37f};
    if (def.id == "rally") return {-1.65f, -0.10f};
    if (def.id == "tank") return {-1.60f, 0.16f};
    return {-1.50f, -0.10f};
}

b2Vec2 exhaustDirection(const VehicleDef& def)
{
    if (def.id == "tractor") return {-0.2f, 1.0f};
    if (def.id == "moonlander") return {0.0f, -1.0f};
    if (def.id == "rally") return {-1.0f, 0.12f};
    if (def.id == "tank") return {-1.0f, 0.35f};
    return {-1.0f, 0.15f};
}

} // namespace hr
