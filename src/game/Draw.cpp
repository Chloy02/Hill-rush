#include "game/Draw.hpp"

#include "core/Math.hpp"

#include <cmath>

namespace hr::draw {

namespace {

float cross(b2Vec2 o, b2Vec2 a, b2Vec2 b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool pointInTriangle(b2Vec2 p, b2Vec2 a, b2Vec2 b, b2Vec2 c)
{
    const float d1 = cross(a, b, p);
    const float d2 = cross(b, c, p);
    const float d3 = cross(c, a, p);
    const bool hasNeg = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool hasPos = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(hasNeg && hasPos);
}

} // namespace

std::vector<int> triangulate(const std::vector<b2Vec2>& polygon)
{
    std::vector<int> result;
    const int n = static_cast<int>(polygon.size());
    if (n < 3) return result;

    // Work counter-clockwise.
    float area = 0.0f;
    for (int i = 0; i < n; ++i) {
        const b2Vec2 a = polygon[static_cast<size_t>(i)];
        const b2Vec2 b = polygon[static_cast<size_t>((i + 1) % n)];
        area += a.x * b.y - b.x * a.y;
    }
    std::vector<int> idx(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) idx[static_cast<size_t>(i)] = area > 0.0f ? i : n - 1 - i;

    int guard = 0;
    while (idx.size() > 3 && guard++ < 10000) {
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m; ++i) {
            const int ia = idx[static_cast<size_t>((i + m - 1) % m)];
            const int ib = idx[static_cast<size_t>(i)];
            const int ic = idx[static_cast<size_t>((i + 1) % m)];
            const b2Vec2 a = polygon[static_cast<size_t>(ia)];
            const b2Vec2 b = polygon[static_cast<size_t>(ib)];
            const b2Vec2 c = polygon[static_cast<size_t>(ic)];
            if (cross(a, b, c) <= 1e-9f) continue;  // reflex or degenerate corner

            bool contains = false;
            for (int j = 0; j < m && !contains; ++j) {
                const int ip = idx[static_cast<size_t>(j)];
                if (ip == ia || ip == ib || ip == ic) continue;
                contains = pointInTriangle(polygon[static_cast<size_t>(ip)], a, b, c);
            }
            if (contains) continue;

            result.push_back(ia);
            result.push_back(ib);
            result.push_back(ic);
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;  // malformed polygon: give up on the rest
    }
    if (idx.size() == 3) {
        result.push_back(idx[0]);
        result.push_back(idx[1]);
        result.push_back(idx[2]);
    }
    return result;
}

void fill(const Shape& shape, b2Transform t, Color color)
{
    for (size_t i = 0; i + 2 < shape.triangles.size(); i += 3) {
        const Vector2 a = rv(xf(t, shape.points[static_cast<size_t>(shape.triangles[i])]));
        const Vector2 b = rv(xf(t, shape.points[static_cast<size_t>(shape.triangles[i + 1])]));
        const Vector2 c = rv(xf(t, shape.points[static_cast<size_t>(shape.triangles[i + 2])]));
        DrawTriangle(a, b, c, color);
    }
}

void outline(const Shape& shape, b2Transform t, float width, Color color)
{
    const size_t n = shape.points.size();
    for (size_t i = 0; i < n; ++i) {
        line(xf(t, shape.points[i]), xf(t, shape.points[(i + 1) % n]), width, color);
    }
}

void fillOutlined(const Shape& shape, b2Transform t, Color fillColor, Color lineColor, float width)
{
    fill(shape, t, fillColor);
    outline(shape, t, width, lineColor);
}

void line(b2Vec2 a, b2Vec2 b, float width, Color color)
{
    const Vector2 ra = rv(a);
    const Vector2 rb = rv(b);
    DrawLineEx(ra, rb, width, color);
    DrawCircleV(ra, width * 0.5f, color);
    DrawCircleV(rb, width * 0.5f, color);
}

void circle(b2Vec2 centre, float radius, Color color)
{
    DrawCircleV(rv(centre), radius, color);
}

void circleOutlined(b2Vec2 centre, float radius, Color fillColor, Color lineColor, float width)
{
    DrawCircleV(rv(centre), radius + width * 0.5f, lineColor);
    DrawCircleV(rv(centre), radius - width * 0.5f, fillColor);
}

void screenLine(Vector2 a, Vector2 b, float width, Color color)
{
    DrawLineEx(a, b, width, color);
    DrawCircleV(a, width * 0.5f, color);
    DrawCircleV(b, width * 0.5f, color);
}

Color withAlpha(Color c, float alpha)
{
    c.a = static_cast<unsigned char>(clampf(alpha, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}

Color mix(Color a, Color b, float t)
{
    t = clampf(t, 0.0f, 1.0f);
    auto lerpByte = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t));
    };
    return {lerpByte(a.r, b.r), lerpByte(a.g, b.g), lerpByte(a.b, b.b), lerpByte(a.a, b.a)};
}

Color shade(Color c, float factor)
{
    auto scale = [factor](unsigned char v) {
        return static_cast<unsigned char>(clampf(static_cast<float>(v) * factor, 0.0f, 255.0f));
    };
    return {scale(c.r), scale(c.g), scale(c.b), c.a};
}

} // namespace hr::draw
