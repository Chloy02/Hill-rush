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
    // Sidewall highlight.
    DrawRing(draw::rv(c), r * 0.70f, r * 0.80f, 0.0f, 360.0f, 32, draw::shade(s.tire, 1.35f));
    // Rim and spokes.
    const float rr = r * s.rimScale;
    draw::circle(c, rr + kLine * 0.5f, kInk);
    draw::circle(c, rr, s.rim);
    DrawRing(draw::rv(c), rr * 0.72f, rr * 0.86f, 0.0f, 360.0f, 32, draw::shade(s.rim, 0.85f));
    for (int i = 0; i < s.spokes; ++i) {
        const float a = static_cast<float>(i) / static_cast<float>(s.spokes) * kTwoPi;
        const b2Vec2 dir = b2RotateVector(t.q, {std::cos(a), std::sin(a)});
        draw::line(b2MulAdd(c, rr * 0.25f, dir), b2MulAdd(c, rr * 0.78f, dir), rr * 0.16f, draw::shade(s.rim, 0.75f));
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

void drawHead(b2Transform t, float r, Headgear gear, Color gearColor, Color accent, bool crashed)
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
        if (gear == Headgear::FullFace || gear == Headgear::RaceHelmet) {
            Shape visor(std::vector<b2Vec2>{{0.15f * r, 0.12f * r}, {1.12f * r, 0.18f * r}, {1.05f * r, -0.30f * r},
                                            {0.20f * r, -0.28f * r}});
            draw::fillOutlined(visor, t, {46, 62, 96, 255}, kInk, kLine * 0.8f);
            draw::line(P(0.35f, 0.04f), P(0.85f, 0.08f), r * 0.10f, {150, 190, 240, 200});
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
    }
    return s;
}

} // namespace

void drawVehicle(const VehicleDef& def, const VehiclePose& pose, float time)
{
    (void)time;
    const WheelStyle wheelStyle = wheelStyleFor(def.id);

    // Struts for the vehicles that show them, then wheels behind the bike frame.
    if (def.id == "jeep" || def.id == "racecar" || def.id == "tractor") {
        for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i) {
            drawStrut(b2TransformPoint(pose.chassis, {def.wheels[i].anchor.x, def.wheels[i].anchor.y + 0.30f}),
                      pose.wheels[i].p, 0.07f, kSteelDark, def.id == "jeep");
        }
    }
    const bool wheelsBehind = def.id == "bike";
    auto drawWheels = [&] {
        for (size_t i = 0; i < pose.wheels.size() && i < def.wheels.size(); ++i)
            drawWheel(pose.wheels[i], def.wheels[i].radius, wheelStyle);
    };
    if (wheelsBehind) drawWheels();

    Headgear gear = Headgear::Helmet;
    Color gearColor{250, 204, 52, 255};
    Color accent{220, 60, 50, 255};
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
    }

    if (!wheelsBehind) drawWheels();
    drawHead(pose.head, def.headRadius, gear, gearColor, accent, pose.crashed);
}

b2Vec2 exhaustPoint(const VehicleDef& def)
{
    if (def.id == "bike") return {-0.88f, 0.16f};
    if (def.id == "tractor") return {0.78f, 1.12f};
    if (def.id == "monster") return {-1.60f, 0.10f};
    if (def.id == "racecar") return {-1.80f, 0.10f};
    return {-1.50f, -0.10f};
}

b2Vec2 exhaustDirection(const VehicleDef& def)
{
    if (def.id == "tractor") return {-0.2f, 1.0f};
    return {-1.0f, 0.15f};
}

} // namespace hr
