#pragma once

#include <box2d/math_functions.h>
#include <raylib.h>

#include <vector>

namespace hr::draw {

// World space is metres with y up. Inside BeginMode2D we draw in "render
// space": the same metres but with y pointing down, like the screen.
inline Vector2 rv(b2Vec2 p) { return {p.x, -p.y}; }
inline Vector2 rv(float x, float y) { return {x, -y}; }
inline b2Vec2 xf(b2Transform t, b2Vec2 local) { return b2TransformPoint(t, local); }

// Ear-clipping triangulation of a simple polygon (either winding).
// Returns triangle vertex indices, three per triangle.
std::vector<int> triangulate(const std::vector<b2Vec2>& polygon);

// A polygon in some local frame, triangulated once and drawn many times.
struct Shape {
    std::vector<b2Vec2> points;
    std::vector<int> triangles;

    Shape() = default;
    explicit Shape(std::vector<b2Vec2> pts) : points(std::move(pts)), triangles(triangulate(points)) {}
};

void fill(const Shape& shape, b2Transform t, Color color);
void outline(const Shape& shape, b2Transform t, float width, Color color);
// Filled shape with an outline of the given world width.
void fillOutlined(const Shape& shape, b2Transform t, Color fill, Color line, float width);

// Thick world-space line with round caps.
void line(b2Vec2 a, b2Vec2 b, float width, Color color);
void circle(b2Vec2 centre, float radius, Color color);
void circleOutlined(b2Vec2 centre, float radius, Color fill, Color line, float width);

// Screen-space helpers -------------------------------------------------------

// Thick line with round caps in screen space.
void screenLine(Vector2 a, Vector2 b, float width, Color color);

Color withAlpha(Color c, float alpha);
Color mix(Color a, Color b, float t);
Color shade(Color c, float factor);  // <1 darker, >1 lighter

} // namespace hr::draw
