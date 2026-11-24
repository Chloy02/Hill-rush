#include "game/OnlineUi.hpp"

#include "game/Hud.hpp"
#include "game/VehicleArt.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

const Color kInk{30, 26, 38, 255};

constexpr float kToastSeconds = 3.0f;

} // namespace

void Toasts::take(std::deque<std::string>& notices)
{
    while (!notices.empty()) {
        items_.push_back({notices.front(), kToastSeconds});
        notices.pop_front();
    }
    while (items_.size() > 3) items_.pop_front();
}

void Toasts::update(float dt)
{
    for (Item& i : items_) i.life -= dt;
    while (!items_.empty() && items_.front().life <= 0.0f) items_.pop_front();
}

void Toasts::draw(Ui& ui, float y, float left) const
{
    if (items_.empty()) return;
    const Item& item = items_.back();
    const float alpha = std::fmin(1.0f, item.life * 2.0f);
    const float size = ui.px(26.0f);
    const Vector2 dim = measureText(ui.assets(), item.text.c_str(), size);
    const float w = dim.x + ui.px(44.0f);
    const float x = left < 0.0f ? ui.width() * 0.5f - w * 0.5f : ui.px(left);
    const Rectangle r{x, ui.px(y), w, size + ui.px(14.0f)};
    DrawRectangleRounded(r, 1.0f, 12, Fade({30, 26, 38, 255}, 0.8f * alpha));
    ui.text(item.text.c_str(), {r.x + r.width * 0.5f, r.y + (r.height - dim.y) * 0.5f}, size,
            Fade({255, 230, 140, 255}, alpha), Align::Center);
}

void drawWrapped(Ui& ui, const std::string& text, float y, float maxWidth, float size, Color color)
{
    std::string line;
    float lineY = ui.px(y);
    const float px = ui.px(size);
    auto flush = [&] {
        if (line.empty()) return;
        ui.text(line.c_str(), {ui.width() * 0.5f, lineY}, px, color, Align::Center);
        lineY += px * 1.2f;
        line.clear();
    };
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos) end = text.size();
        const std::string word = text.substr(start, end - start);
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measureText(ui.assets(), candidate.c_str(), px).x > ui.px(maxWidth)) {
            flush();
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    flush();
}

Color playerColor(uint8_t index)
{
    static const Color colors[] = {
        {66, 146, 226, 255},  // blue
        {250, 150, 30, 255},  // orange
        {232, 70, 140, 255},  // pink
        {146, 92, 226, 255},  // purple
    };
    return colors[index % 4];
}

void outlinedPill(Ui& ui, Rectangle r, Color fill)
{
    const float b = ui.px(3.0f);
    DrawRectangleRounded({r.x - b, r.y - b, r.width + b * 2.0f, r.height + b * 2.0f}, 1.0f, 16, kInk);
    DrawRectangleRounded(r, 1.0f, 16, fill);
}

void drawNameTag(Ui& ui, Vector2 anchor, const char* text, Color color, float size)
{
    const float textSize = ui.px(size);
    const Vector2 dim = measureText(ui.assets(), text, textSize);
    const float padX = ui.px(14.0f);
    const float h = textSize + ui.px(12.0f);
    const Rectangle r{anchor.x - dim.x * 0.5f - padX, anchor.y - h - ui.px(10.0f), dim.x + padX * 2.0f, h};
    const float b = ui.px(3.0f);
    DrawTriangle({anchor.x - ui.px(10.0f), r.y + h - 1.0f}, {anchor.x + ui.px(10.0f), r.y + h - 1.0f}, {anchor.x, anchor.y + b},
                 kInk);
    outlinedPill(ui, r, color);
    DrawTriangle({anchor.x - ui.px(7.0f), r.y + h - 2.0f}, {anchor.x + ui.px(7.0f), r.y + h - 2.0f},
                 {anchor.x, anchor.y - b * 0.5f}, color);
    ui.text(text, {anchor.x, r.y + (h - dim.y) * 0.5f}, textSize, WHITE, Align::Center);
}

void drawAvatar(Ui& ui, Vector2 centre, float radius, const std::string& name, Color color)
{
    DrawCircleV(centre, radius + ui.px(3.0f), kInk);
    DrawCircleV(centre, radius, color);
    const char initial[2] = {name.empty() ? '?' : name[0], 0};
    const float size = radius * 1.3f;
    const Vector2 dim = measureText(ui.assets(), initial, size);
    ui.text(initial, {centre.x, centre.y - dim.y * 0.5f}, size, WHITE, Align::Center);
}

void drawVehicleThumb(Ui& ui, const VehicleDef& def, Vector2 centre, float zoom, float time)
{
    Camera2D cam{};
    cam.target = {0.0f, -0.55f};
    cam.offset = centre;
    cam.zoom = ui.px(zoom);
    BeginMode2D(cam);
    drawVehicle(def, restPose(def, {0.0f, 0.0f}, 0.0f), time);
    EndMode2D();
}

std::string goalText(int goal)
{
    if (goal <= 0) return "FURTHEST WINS";
    return "RACE TO " + formatThousands(goal) + "m";
}

std::string raceTimeText(float seconds)
{
    const int tenths = static_cast<int>(std::lround(std::fmax(0.0f, seconds) * 10.0f));
    char text[32];
    std::snprintf(text, sizeof(text), "%d:%02d.%d", tenths / 600, (tenths / 10) % 60, tenths % 10);
    return text;
}

const char* placeText(int place)
{
    switch (place) {
    case 1: return "1ST";
    case 2: return "2ND";
    case 3: return "3RD";
    default: return "4TH";
    }
}

Color placeColor(int place)
{
    switch (place) {
    case 1: return {255, 214, 60, 255};
    case 2: return {214, 222, 236, 255};
    case 3: return {222, 150, 90, 255};
    default: return {190, 198, 218, 255};
    }
}

} // namespace hr
