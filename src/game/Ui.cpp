#include "game/Ui.hpp"

#include "core/Math.hpp"
#include "game/Draw.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace hr {

namespace {

struct ButtonColors {
    Color face;
    Color edge;
};

ButtonColors colorsFor(ButtonKind kind)
{
    switch (kind) {
    case ButtonKind::Go: return {{94, 186, 52, 255}, {52, 120, 30, 255}};
    case ButtonKind::Normal: return {{252, 172, 38, 255}, {190, 112, 16, 255}};
    case ButtonKind::Back: return {{226, 74, 62, 255}, {150, 40, 32, 255}};
    case ButtonKind::Info: return {{66, 146, 226, 255}, {36, 92, 160, 255}};
    case ButtonKind::Buy: return {{255, 204, 40, 255}, {196, 140, 10, 255}};
    }
    return {{200, 200, 200, 255}, {120, 120, 120, 255}};
}

const Color kDisabledFace{150, 150, 158, 255};
const Color kDisabledEdge{100, 100, 108, 255};
const Color kOutline{30, 26, 38, 255};

} // namespace

uint64_t Ui::hashId(const char* id)
{
    uint64_t h = 1469598103934665603ull;
    for (const char* p = id; *p; ++p) {
        h ^= static_cast<unsigned char>(*p);
        h *= 1099511628211ull;
    }
    return h;
}

void Ui::beginFrame(Input& input, const Assets& assets)
{
    input_ = &input;
    assets_ = &assets;
    width_ = static_cast<float>(GetScreenWidth());
    height_ = static_cast<float>(GetScreenHeight());
    scale_ = std::fmin(width_ / kLogicalW, height_ / kLogicalH);
    lastWidgets_.swap(widgets_);
    widgets_.clear();
    consumeConfirm_ = false;
    lockHorizontal_ = false;
    // Keep the pressed widget through the release frame so the click can land.
    if (!input.mouseDown() && !input.mouseReleased()) pressedId_ = 0;
}

void Ui::endFrame()
{
    // A text field that's gone (the screen changed) can't keep the keyboard.
    if (editing_ && std::none_of(widgets_.begin(), widgets_.end(), [this](const Widget& w) { return w.id == editing_; })) {
        editing_ = 0;
    }
    if (widgets_.empty() || editing_) return;  // typing: the arrow keys don't move focus

    bool focusValid = false;
    const Widget* current = nullptr;
    for (const Widget& w : widgets_) {
        if (w.id == focus_) {
            focusValid = true;
            current = &w;
        }
    }
    if (!focusValid) {
        focus_ = 0;
        for (const Widget& w : widgets_) {
            if (w.id == defaultFocus_) {
                focus_ = w.id;
                current = &w;
            }
        }
        if (!current) {
            focus_ = widgets_.front().id;
            current = &widgets_.front();
        }
    }

    // Spatial navigation: pick the nearest widget in the pressed direction.
    float dirX = 0.0f, dirY = 0.0f;
    if (input_->pressed(Action::Up)) dirY = -1.0f;
    else if (input_->pressed(Action::Down)) dirY = 1.0f;
    else if (!current->horizontal && !lockHorizontal_ && input_->pressed(Action::Left)) dirX = -1.0f;
    else if (!current->horizontal && !lockHorizontal_ && input_->pressed(Action::Right)) dirX = 1.0f;
    if (dirX == 0.0f && dirY == 0.0f) return;

    const float cx = current->rect.x + current->rect.width * 0.5f;
    const float cy = current->rect.y + current->rect.height * 0.5f;
    float bestScore = 1e30f;
    uint64_t best = 0;
    for (const Widget& w : widgets_) {
        if (w.id == current->id) continue;
        const float wx = w.rect.x + w.rect.width * 0.5f - cx;
        const float wy = w.rect.y + w.rect.height * 0.5f - cy;
        const float along = wx * dirX + wy * dirY;
        if (along <= 1.0f) continue;
        const float across = std::fabs(wx * dirY - wy * dirX);
        const float score = along + across * 2.5f;
        if (score < bestScore) {
            bestScore = score;
            best = w.id;
        }
    }
    if (best != 0) focus_ = best;
}

Rectangle Ui::centered(float dx, float dy, float w, float h) const
{
    return {width_ * 0.5f + px(dx - w * 0.5f), height_ * 0.5f + px(dy - h * 0.5f), px(w), px(h)};
}

Rectangle Ui::anchored(float ax, float ay, float dx, float dy, float w, float h) const
{
    return {width_ * ax + px(dx) - px(w) * ax, height_ * ay + px(dy) - px(h) * ay, px(w), px(h)};
}

bool Ui::hovered(Rectangle r) const
{
    return CheckCollisionPointRec(input_->mouse(), r);
}

void Ui::registerWidget(uint64_t id, Rectangle r, bool horizontal)
{
    widgets_.push_back({id, r, horizontal});
}

void Ui::setFocus(const char* id) { focus_ = hashId(id); }

bool Ui::activated(uint64_t id, Rectangle r, bool enabled)
{
    if (!enabled) return false;
    bool fire = false;
    if (hovered(r)) {
        if (input_->mousePressed()) pressedId_ = id;
        if (input_->mouseReleased() && pressedId_ == id) fire = true;
        if (input_->mouseMoved() && !editing_) focus_ = id;
    }
    // While typing, Space and Enter belong to the text field, not to the focused button.
    if (!editing_ && !consumeConfirm_ && focus_ == id && input_->usingKeys() && input_->pressed(Action::Confirm)) {
        fire = true;
        consumeConfirm_ = true;
    }
    if (fire) {
        pressedId_ = 0;
        if (onClick) onClick();
    }
    return fire;
}

void Ui::drawFocus(Rectangle r) const
{
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 6.0f);
    const float grow = px(6.0f + 2.0f * pulse);
    const Rectangle o{r.x - grow, r.y - grow, r.width + grow * 2.0f, r.height + grow * 2.0f};
    DrawRectangleRoundedLinesEx(o, 0.35f, 8, px(3.0f), draw::withAlpha(WHITE, 0.65f + 0.35f * pulse));
}

bool Ui::button(const char* idText, Rectangle r, const char* label, ButtonKind kind, bool enabled, float fontSize)
{
    const uint64_t id = hashId(idText);
    registerWidget(id, r, false);
    const bool fire = activated(id, r, enabled);

    const bool hover = enabled && hovered(r);
    const bool pressed = enabled && pressedId_ == id && hover && input_->mouseDown();
    ButtonColors c = colorsFor(kind);
    if (!enabled) c = {kDisabledFace, kDisabledEdge};
    else if (hover) c.face = draw::shade(c.face, 1.08f);

    const float depth = px(pressed ? 2.0f : 6.0f);
    const float lift = px(pressed ? 4.0f : 0.0f);
    const float roundness = 0.38f;
    const float border = px(3.0f);

    const Rectangle base{r.x, r.y + lift, r.width, r.height - px(4.0f)};
    const Rectangle edge{base.x, base.y + depth, base.width, base.height};
    DrawRectangleRounded({edge.x - border, edge.y - border, edge.width + border * 2, edge.height + border * 2},
                         roundness, 10, kOutline);
    DrawRectangleRounded({base.x - border, base.y - border, base.width + border * 2, base.height + border * 2},
                         roundness, 10, kOutline);
    DrawRectangleRounded(edge, roundness, 10, c.edge);
    DrawRectangleRounded(base, roundness, 10, c.face);
    // Gloss on the top half.
    DrawRectangleRounded({base.x + px(6), base.y + px(4), base.width - px(12), base.height * 0.38f}, 0.6f, 8,
                         draw::withAlpha(WHITE, 0.28f));

    if (focus_ == id && input_->usingKeys() && enabled) drawFocus(r);

    const float size = fontSize > 0.0f ? px(fontSize) : std::fmin(base.height * 0.52f, px(40.0f));
    TextStyle style;
    style.size = size;
    style.color = enabled ? WHITE : Color{225, 225, 230, 255};
    style.outline = kOutline;
    style.outlineWidth = std::fmax(1.5f, size * 0.07f);
    const Vector2 dim = measureText(*assets_, label, size, style.spacing);
    drawText(*assets_, label, {base.x + base.width * 0.5f, base.y + (base.height - dim.y) * 0.5f}, style, Align::Center);
    return fire;
}

bool Ui::arrowButton(const char* idText, Rectangle r, int dir, bool enabled)
{
    // Mouse/touch only: keyboards page carousels with Left/Right directly.
    const uint64_t id = hashId(idText);
    const bool fire = activated(id, r, enabled);
    const bool hover = enabled && hovered(r);
    const bool pressed = enabled && pressedId_ == id && hover && input_->mouseDown();

    const float cx = r.x + r.width * 0.5f;
    const float cy = r.y + r.height * 0.5f + (pressed ? px(3) : 0.0f);
    const float w = r.width * 0.42f;
    const float h = r.height * 0.46f;
    const float d = static_cast<float>(dir);
    Color face = enabled ? Color{255, 196, 40, 255} : kDisabledFace;
    if (hover) face = draw::shade(face, 1.1f);
    const Vector2 tip{cx + d * w, cy};
    const Vector2 top{cx - d * w, cy - h};
    const Vector2 bottom{cx - d * w, cy + h};
    // Outline by drawing a slightly bigger triangle first.
    const float o = px(5.0f);
    const Vector2 tipO{tip.x + d * o * 1.6f, tip.y};
    const Vector2 topO{top.x - d * o, top.y - o * 1.4f};
    const Vector2 bottomO{bottom.x - d * o, bottom.y + o * 1.4f};
    // Backface culling is disabled globally, so winding doesn't matter here.
    DrawTriangle(tipO, topO, bottomO, kOutline);
    DrawTriangle(tip, top, bottom, face);
    return fire;
}

float Ui::slider(const char* idText, Rectangle r, float value)
{
    const uint64_t id = hashId(idText);
    registerWidget(id, r, true);
    const bool hover = hovered(r);
    if (hover && input_->mousePressed()) pressedId_ = id;
    if (pressedId_ == id && input_->mouseDown()) {
        value = clampf((input_->mouse().x - r.x) / r.width, 0.0f, 1.0f);
        focus_ = id;
    }
    if (focus_ == id && input_->usingKeys()) {
        if (input_->pressed(Action::Left)) {
            value = clampf(value - 0.1f, 0.0f, 1.0f);
            if (onClick) onClick();
        }
        if (input_->pressed(Action::Right)) {
            value = clampf(value + 0.1f, 0.0f, 1.0f);
            if (onClick) onClick();
        }
    }
    if (input_->mouseReleased() && pressedId_ == id && onClick) onClick();

    const float trackH = r.height * 0.32f;
    const Rectangle track{r.x, r.y + (r.height - trackH) * 0.5f, r.width, trackH};
    DrawRectangleRounded({track.x - px(3), track.y - px(3), track.width + px(6), track.height + px(6)}, 1.0f, 8, kOutline);
    DrawRectangleRounded(track, 1.0f, 8, {70, 70, 84, 255});
    DrawRectangleRounded({track.x, track.y, track.width * value, track.height}, 1.0f, 8, {94, 186, 52, 255});
    const Vector2 knob{r.x + r.width * value, r.y + r.height * 0.5f};
    DrawCircleV(knob, r.height * 0.45f + px(3), kOutline);
    DrawCircleV(knob, r.height * 0.45f, {255, 204, 40, 255});
    DrawCircleV({knob.x - r.height * 0.12f, knob.y - r.height * 0.12f}, r.height * 0.14f, draw::withAlpha(WHITE, 0.5f));
    if (focus_ == id && input_->usingKeys()) drawFocus(r);
    return value;
}

bool Ui::toggle(const char* idText, Rectangle r, bool value)
{
    const uint64_t id = hashId(idText);
    registerWidget(id, r, false);
    if (activated(id, r, true)) value = !value;

    const Rectangle body{r.x, r.y, r.height * 1.9f, r.height};
    DrawRectangleRounded({body.x - px(3), body.y - px(3), body.width + px(6), body.height + px(6)}, 1.0f, 8, kOutline);
    DrawRectangleRounded(body, 1.0f, 8, value ? Color{94, 186, 52, 255} : Color{110, 110, 124, 255});
    const float knobX = value ? body.x + body.width - body.height * 0.5f : body.x + body.height * 0.5f;
    DrawCircleV({knobX, body.y + body.height * 0.5f}, body.height * 0.40f, WHITE);
    if (focus_ == id && input_->usingKeys()) drawFocus(body);
    return value;
}

bool Ui::textField(const char* idText, Rectangle r, std::string& value, size_t maxLength, TextKind kind,
                   const char* placeholder)
{
    const uint64_t id = hashId(idText);
    registerWidget(id, r, false);
    bool finished = false;
    if (editing_ == id) {
        for (char c : input_->typed()) {
            if (value.size() >= maxLength) break;
            const unsigned char u = static_cast<unsigned char>(c);
            if (kind == TextKind::Name) {
                if (std::isalnum(u) || c == ' ' || c == '-') value += static_cast<char>(std::toupper(u));
            } else if (std::isalnum(u) || c == '.' || c == ':' || c == '-' || c == '_') {
                value += c;
            }
        }
        for (int i = 0; i < input_->backspaces() && !value.empty(); ++i) value.pop_back();
        const bool clickedAway = input_->mousePressed() && !hovered(r);
        if (input_->enterPressed() || input_->escapePressed() || clickedAway) {
            editing_ = 0;
            finished = true;
            consumeConfirm_ = true;  // the Enter that finished typing presses nothing else
        }
    } else if (activated(id, r, true)) {
        editing_ = id;
        focus_ = id;
    }

    const bool active = editing_ == id;
    const float border = px(3.0f);
    DrawRectangleRounded({r.x - border, r.y - border, r.width + border * 2, r.height + border * 2}, 0.3f, 10, kOutline);
    DrawRectangleRounded(r, 0.3f, 10, active ? WHITE : (hovered(r) ? Color{238, 241, 248, 255} : Color{222, 227, 238, 255}));
    TextStyle style;
    style.size = r.height * 0.52f;
    style.color = kOutline;
    const bool showPlaceholder = value.empty() && !active;
    if (showPlaceholder) style.color = {136, 142, 158, 255};
    const char* shown = showPlaceholder ? placeholder : value.c_str();
    const Vector2 dim = measureText(*assets_, shown[0] ? shown : "M", style.size, style.spacing);
    const Vector2 at{r.x + px(14.0f), r.y + (r.height - dim.y) * 0.5f};
    drawText(*assets_, shown, at, style, Align::Left);
    if (active && std::fmod(GetTime(), 1.0) < 0.6) {
        const float x = at.x + (value.empty() ? 0.0f : measureText(*assets_, value.c_str(), style.size, style.spacing).x) + px(2.0f);
        DrawRectangleRec({x, r.y + r.height * 0.2f, px(3.0f), r.height * 0.6f}, kOutline);
    }
    if (!active && focus_ == id && input_->usingKeys()) drawFocus(r);
    return finished;
}

void Ui::panel(Rectangle r, Color fill, Color border, float roundness)
{
    const float b = px(4.0f);
    DrawRectangleRounded({r.x - b, r.y - b + px(6), r.width + 2 * b, r.height + 2 * b}, roundness, 10,
                         draw::withAlpha(BLACK, 0.25f));
    DrawRectangleRounded({r.x - b, r.y - b, r.width + 2 * b, r.height + 2 * b}, roundness, 10, border);
    DrawRectangleRounded(r, roundness, 10, fill);
}

void Ui::text(const char* s, Vector2 pos, float size, Color color, Align align, bool outlined)
{
    TextStyle style;
    style.size = size;
    style.color = color;
    if (outlined) {
        style.outline = draw::withAlpha(kOutline, static_cast<float>(color.a) / 255.0f);
        style.outlineWidth = std::fmax(1.5f, size * 0.07f);
    }
    drawText(*assets_, s, pos, style, align);
}

} // namespace hr
