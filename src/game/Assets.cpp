#include "game/Assets.hpp"

#include "core/Embedded.hpp"
#include "core/Log.hpp"
#include "core/Math.hpp"
#include "core/Rng.hpp"
#include "game/Draw.hpp"

#include <cmath>

namespace hr {

namespace {

Font loadEmbeddedFont(int size)
{
    const embedded::Blob* blob = embedded::find("font_main");
    if (!blob) {
        LOG_WARN("Embedded font missing; falling back to raylib default font");
        return GetFontDefault();
    }
    Font f = LoadFontFromMemory(".ttf", blob->data, static_cast<int>(blob->size), size, nullptr, 0);
    if (f.texture.id == 0) {
        LOG_WARN("Could not load embedded font; using default");
        return GetFontDefault();
    }
    GenTextureMipmaps(&f.texture);
    SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
    return f;
}

// Speckled noise texture, mostly white so it can be tinted by stage colour.
Texture2D makeGroundTexture()
{
    constexpr int kSize = 256;
    Image img = GenImageColor(kSize, kSize, WHITE);
    Color* px = static_cast<Color*>(img.data);
    Rng rng(0x5EED);
    // Soft value noise base.
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float n = 0.5f + 0.5f * std::sin(static_cast<float>(x) * 0.11f + std::sin(static_cast<float>(y) * 0.07f) * 2.0f) *
                                       std::cos(static_cast<float>(y) * 0.09f + std::cos(static_cast<float>(x) * 0.05f) * 2.0f);
            const unsigned char v = static_cast<unsigned char>(225 + 30 * n);
            px[y * kSize + x] = {v, v, v, 255};
        }
    }
    // Pebbles and speckles (wrap-around so the texture tiles seamlessly).
    for (int i = 0; i < 420; ++i) {
        const int cx = rng.rangeInt(0, kSize - 1);
        const int cy = rng.rangeInt(0, kSize - 1);
        const int r = rng.chance(0.15f) ? rng.rangeInt(4, 9) : rng.rangeInt(1, 3);
        const unsigned char v = static_cast<unsigned char>(rng.chance(0.5f) ? rng.rangeInt(150, 190) : rng.rangeInt(235, 255));
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (dx * dx + dy * dy > r * r) continue;
                const int xx = (cx + dx + kSize) % kSize;
                const int yy = (cy + dy + kSize) % kSize;
                const float edge = static_cast<float>(dx * dx + dy * dy) / static_cast<float>(r * r + 1);
                const unsigned char shaded = static_cast<unsigned char>(static_cast<float>(v) * (1.0f - 0.15f * edge));
                px[yy * kSize + xx] = {shaded, shaded, shaded, 255};
            }
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    return tex;
}

// Horizontal streaks for the grass/sand strip; tiles along x.
Texture2D makeSurfaceTexture()
{
    constexpr int kW = 256;
    constexpr int kH = 64;
    Image img = GenImageColor(kW, kH, WHITE);
    Color* px = static_cast<Color*>(img.data);
    Rng rng(0xB1ADE);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const float stripe = 0.5f + 0.5f * std::sin(static_cast<float>(x) * kTwoPi * 7.0f / kW + static_cast<float>(y) * 0.15f);
            const unsigned char v = static_cast<unsigned char>(236 + 19 * stripe);
            px[y * kW + x] = {v, v, v, 255};
        }
    }
    // Blades: short vertical darker dashes.
    for (int i = 0; i < 260; ++i) {
        const int x = rng.rangeInt(0, kW - 1);
        const int y0 = rng.rangeInt(0, kH - 8);
        const int len = rng.rangeInt(3, 10);
        const unsigned char v = static_cast<unsigned char>(rng.rangeInt(190, 220));
        for (int y = y0; y < y0 + len && y < kH; ++y) px[y * kW + x] = {v, v, v, 255};
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
    SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
    return tex;
}

Texture2D makeVignette()
{
    constexpr int kSize = 256;
    Image img = GenImageColor(kSize, kSize, BLANK);
    Color* px = static_cast<Color*>(img.data);
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / kSize * 2.0f - 1.0f;
            const float dy = (static_cast<float>(y) + 0.5f) / kSize * 2.0f - 1.0f;
            const float d = std::sqrt(dx * dx + dy * dy) / std::sqrt(2.0f);
            const float a = smoothstep(0.45f, 1.0f, d);
            px[y * kSize + x] = {0, 0, 0, static_cast<unsigned char>(a * 200.0f)};
        }
    }
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

void blendPixel(Image& img, int x, int y, Color c)
{
    if (x < 0 || y < 0 || x >= img.width || y >= img.height) return;
    Color* px = static_cast<Color*>(img.data);
    Color& d = px[y * img.width + x];
    const float a = static_cast<float>(c.a) / 255.0f;
    d.r = static_cast<unsigned char>(d.r + (c.r - d.r) * a);
    d.g = static_cast<unsigned char>(d.g + (c.g - d.g) * a);
    d.b = static_cast<unsigned char>(d.b + (c.b - d.b) * a);
    d.a = static_cast<unsigned char>(std::fmin(255.0f, d.a + c.a * (1.0f - d.a / 255.0f)));
}

// Anti-aliased filled circle via 4x4 supersampling.
void iconCircle(Image& img, float cx, float cy, float r, Color c)
{
    const int x0 = static_cast<int>(std::floor(cx - r - 1)), x1 = static_cast<int>(std::ceil(cx + r + 1));
    const int y0 = static_cast<int>(std::floor(cy - r - 1)), y1 = static_cast<int>(std::ceil(cy + r + 1));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            int inside = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float px = static_cast<float>(x) + (static_cast<float>(sx) + 0.5f) / 4.0f - cx;
                    const float py = static_cast<float>(y) + (static_cast<float>(sy) + 0.5f) / 4.0f - cy;
                    inside += px * px + py * py <= r * r ? 1 : 0;
                }
            }
            if (inside) blendPixel(img, x, y, draw::withAlpha(c, static_cast<float>(inside) / 16.0f));
        }
    }
}

// Anti-aliased convex polygon fill (points in pixel coords, any winding).
void iconPolygon(Image& img, const std::vector<Vector2>& pts, Color c)
{
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (const Vector2& p : pts) {
        minX = std::fmin(minX, p.x);
        maxX = std::fmax(maxX, p.x);
        minY = std::fmin(minY, p.y);
        maxY = std::fmax(maxY, p.y);
    }
    auto inside = [&](float x, float y) {
        bool in = false;
        for (size_t i = 0, j = pts.size() - 1; i < pts.size(); j = i++) {
            const Vector2 a = pts[i], b = pts[j];
            if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
        }
        return in;
    };
    for (int y = static_cast<int>(minY) - 1; y <= static_cast<int>(maxY) + 1; ++y) {
        for (int x = static_cast<int>(minX) - 1; x <= static_cast<int>(maxX) + 1; ++x) {
            int count = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    count += inside(static_cast<float>(x) + (sx + 0.5f) / 4.0f, static_cast<float>(y) + (sy + 0.5f) / 4.0f) ? 1 : 0;
                }
            }
            if (count) blendPixel(img, x, y, draw::withAlpha(c, static_cast<float>(count) / 16.0f));
        }
    }
}

} // namespace

Image makeIconImage(int size)
{
    Image img = GenImageColor(size, size, BLANK);
    const float s = static_cast<float>(size) / 256.0f;
    auto P = [s](float x, float y) { return Vector2{x * s, y * s}; };

    // Rounded-square badge with sky gradient.
    const float radius = 52.0f * s;
    for (int y = 0; y < size; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(size);
        const Color sky = draw::mix({66, 150, 228, 255}, {186, 228, 252, 255}, t);
        for (int x = 0; x < size; ++x) {
            // Distance outside the rounded rect (8px inset).
            const float inset = 8.0f * s;
            const float qx = std::fmax(std::fabs(static_cast<float>(x) + 0.5f - size * 0.5f) - (size * 0.5f - inset - radius), 0.0f);
            const float qy = std::fmax(std::fabs(static_cast<float>(y) + 0.5f - size * 0.5f) - (size * 0.5f - inset - radius), 0.0f);
            const float d = std::sqrt(qx * qx + qy * qy) - radius;
            const float a = clampf(0.5f - d, 0.0f, 1.0f);
            if (a > 0.0f) blendPixel(img, x, y, draw::withAlpha(sky, a));
        }
    }
    // Sun.
    iconCircle(img, 190 * s, 70 * s, 26 * s, {255, 236, 140, 255});
    // Hill.
    std::vector<Vector2> hill;
    for (int i = 0; i <= 40; ++i) {
        const float x = 8.0f + 240.0f * static_cast<float>(i) / 40.0f;
        const float y = 196.0f - 34.0f * std::sin((x - 8.0f) / 240.0f * kPi * 1.1f + 0.25f);
        hill.push_back(P(x, y));
    }
    hill.push_back(P(248, 248));
    hill.push_back(P(8, 248));
    // Clip the hill to the badge by intersecting with alpha later: draw then mask.
    iconPolygon(img, hill, {108, 186, 60, 255});
    // Jeep body.
    iconPolygon(img, {P(70, 150), P(176, 128), P(186, 146), P(182, 162), P(80, 182), P(66, 170)}, {214, 56, 48, 255});
    iconPolygon(img, {P(84, 146), P(92, 112), P(100, 112), P(94, 146)}, {60, 60, 70, 255});
    // Driver head + helmet.
    iconCircle(img, 118 * s, 112 * s, 17 * s, {242, 196, 155, 255});
    iconPolygon(img, {P(100, 110), P(104, 96), P(118, 90), P(132, 96), P(136, 108)}, {250, 206, 60, 255});
    // Wheels.
    for (Vector2 c : {P(92, 182), P(166, 160)}) {
        iconCircle(img, c.x, c.y, 22 * s, {40, 40, 46, 255});
        iconCircle(img, c.x, c.y, 11 * s, {200, 200, 210, 255});
        iconCircle(img, c.x, c.y, 4 * s, {90, 90, 100, 255});
    }
    // Mask everything to the rounded badge.
    Color* px = static_cast<Color*>(img.data);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float inset = 8.0f * s;
            const float qx = std::fmax(std::fabs(static_cast<float>(x) + 0.5f - size * 0.5f) - (size * 0.5f - inset - radius), 0.0f);
            const float qy = std::fmax(std::fabs(static_cast<float>(y) + 0.5f - size * 0.5f) - (size * 0.5f - inset - radius), 0.0f);
            const float d = std::sqrt(qx * qx + qy * qy) - radius;
            const float a = clampf(0.5f - d, 0.0f, 1.0f);
            px[y * size + x].a = static_cast<unsigned char>(static_cast<float>(px[y * size + x].a) * a);
        }
    }
    return img;
}

void Assets::load()
{
    // stb_truetype warns that '|' is taller than the requested size; harmless.
    SetTraceLogLevel(LOG_ERROR);
    fontSmall_ = loadEmbeddedFont(48);
    fontLarge_ = loadEmbeddedFont(128);
    SetTraceLogLevel(LOG_WARNING);
    groundTexture = makeGroundTexture();
    surfaceTexture = makeSurfaceTexture();
    vignette = makeVignette();
    for (int size : {16, 24, 32, 48, 64, 128, 256}) windowIcons.push_back(makeIconImage(size));
}

void Assets::unload()
{
    if (fontSmall_.texture.id != GetFontDefault().texture.id) UnloadFont(fontSmall_);
    if (fontLarge_.texture.id != GetFontDefault().texture.id) UnloadFont(fontLarge_);
    UnloadTexture(groundTexture);
    UnloadTexture(surfaceTexture);
    UnloadTexture(vignette);
    for (Image& image : windowIcons) UnloadImage(image);
    windowIcons.clear();
}

Vector2 measureText(const Assets& assets, const char* text, float size, float spacing)
{
    return MeasureTextEx(assets.font(size), text, size, spacing);
}

void drawText(const Assets& assets, const char* text, Vector2 pos, const TextStyle& style, Align align)
{
    const Font& f = assets.font(style.size);
    const Vector2 dim = MeasureTextEx(f, text, style.size, style.spacing);
    if (align == Align::Center) pos.x -= dim.x * 0.5f;
    else if (align == Align::Right) pos.x -= dim.x;
    pos.x = std::round(pos.x);
    pos.y = std::round(pos.y);

    if (style.shadow.a > 0) {
        const float off = std::fmax(1.0f, style.size * 0.06f);
        DrawTextEx(f, text, {pos.x + off, pos.y + off}, style.size, style.spacing, style.shadow);
    }
    if (style.outline.a > 0 && style.outlineWidth > 0.0f) {
        const float w = style.outlineWidth;
        for (int i = 0; i < 12; ++i) {
            const float a = kTwoPi * static_cast<float>(i) / 12.0f;
            DrawTextEx(f, text, {pos.x + std::cos(a) * w, pos.y + std::sin(a) * w}, style.size, style.spacing, style.outline);
        }
    }
    DrawTextEx(f, text, pos, style.size, style.spacing, style.color);
}

} // namespace hr
