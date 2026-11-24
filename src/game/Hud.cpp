#include "game/Hud.hpp"

#include "core/Math.hpp"
#include "game/Draw.hpp"
#include "game/WorldView.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

const Color kInk{30, 26, 38, 255};
const Color kPanel{20, 20, 30, 150};

void drawOutlinedBar(Rectangle r, float fraction, Color fill, float scale)
{
    const float b = 3.0f * scale;
    DrawRectangleRounded({r.x - b, r.y - b, r.width + 2 * b, r.height + 2 * b}, 0.6f, 8, kInk);
    DrawRectangleRounded(r, 0.6f, 8, {58, 58, 70, 255});
    if (fraction > 0.0f) {
        const Rectangle f{r.x, r.y, std::fmax(r.height, r.width * fraction), r.height};
        DrawRectangleRounded(f, 0.6f, 8, fill);
        DrawRectangleRounded({f.x + 3 * scale, f.y + 2 * scale, f.width - 6 * scale, f.height * 0.35f}, 0.6f, 8,
                             draw::withAlpha(WHITE, 0.35f));
    }
}

void drawJerrycanIcon(Vector2 c, float s)
{
    const Rectangle body{c.x - 13 * s, c.y - 15 * s, 26 * s, 32 * s};
    DrawRectangleRounded({body.x - 3 * s, body.y - 3 * s, body.width + 6 * s, body.height + 6 * s}, 0.25f, 6, kInk);
    DrawRectangleRounded(body, 0.25f, 6, {226, 46, 46, 255});
    DrawRectangle(static_cast<int>(c.x - 9 * s), static_cast<int>(c.y - 21 * s), static_cast<int>(10 * s),
                  static_cast<int>(7 * s), kInk);
    // White fuel drop.
    DrawCircleV({c.x, c.y + 5 * s}, 6 * s, WHITE);
    DrawTriangle({c.x, c.y - 8 * s}, {c.x - 5.6f * s, c.y + 3 * s}, {c.x + 5.6f * s, c.y + 3 * s}, WHITE);
}

void drawCoinIcon(Vector2 c, float r)
{
    DrawCircleV(c, r + 2.5f, kInk);
    DrawCircleV(c, r, {214, 150, 20, 255});
    DrawCircleV(c, r * 0.78f, {255, 204, 40, 255});
    DrawCircleV({c.x - r * 0.25f, c.y - r * 0.3f}, r * 0.2f, draw::withAlpha(WHITE, 0.6f));
}

void drawNitroBadge(Vector2 c, float r, Color fill)
{
    DrawCircleV(c, r + 2.5f, kInk);
    DrawCircleV(c, r, fill);
    const float u = r * 0.68f;
    DrawTriangle({c.x + 0.25f * u, c.y - u}, {c.x - 0.65f * u, c.y + 0.15f * u}, {c.x + 0.15f * u, c.y + 0.15f * u}, WHITE);
    DrawTriangle({c.x - 0.25f * u, c.y + u}, {c.x + 0.65f * u, c.y - 0.15f * u}, {c.x - 0.15f * u, c.y - 0.15f * u}, WHITE);
}

void drawCageBadge(Vector2 c, float s, Color fill)
{
    DrawCircleV(c, 13.0f * s + 2.5f, kInk);
    DrawCircleV(c, 13.0f * s, {42, 46, 62, 255});
    const Rectangle arch{c.x - 7.5f * s, c.y - 6.0f * s, 15.0f * s, 12.0f * s};
    DrawRectangleRoundedLinesEx(arch, 0.45f, 6, 3.0f * s, fill);
    DrawLineEx({c.x - 2.0f * s, c.y - 5.5f * s}, {c.x + 2.5f * s, c.y + 5.5f * s}, 2.4f * s, fill);
}

void drawDial(Vector2 c, float r, float value, const char* label, const char* reading, Ui& ui, bool redZone)
{
    const float s = ui.scale();
    DrawCircleV(c, r + 5 * s, kInk);
    DrawCircleV(c, r, {38, 40, 52, 235});
    DrawRing(c, r * 0.86f, r * 0.94f, 135.0f, 405.0f, 48, {80, 84, 100, 255});
    if (redZone) DrawRing(c, r * 0.80f, r * 0.94f, 345.0f, 405.0f, 24, {220, 60, 50, 255});
    for (int i = 0; i <= 10; ++i) {
        const float a = degToRad(135.0f + 27.0f * static_cast<float>(i));
        const float inner = i % 5 == 0 ? r * 0.68f : r * 0.76f;
        DrawLineEx({c.x + std::cos(a) * inner, c.y + std::sin(a) * inner},
                   {c.x + std::cos(a) * r * 0.86f, c.y + std::sin(a) * r * 0.86f}, 2.5f * s, WHITE);
    }
    const float a = degToRad(135.0f + 270.0f * clampf(value, 0.0f, 1.05f));
    DrawLineEx(c, {c.x + std::cos(a) * r * 0.82f, c.y + std::sin(a) * r * 0.82f}, 4.0f * s, {255, 80, 60, 255});
    DrawCircleV(c, r * 0.12f, {200, 200, 210, 255});
    ui.text(label, {c.x, c.y + r * 0.22f}, r * 0.24f, {200, 204, 220, 255}, Align::Center, false);
    ui.text(reading, {c.x, c.y + r * 0.48f}, r * 0.30f, WHITE, Align::Center, false);
}

} // namespace

Color difficultyColor(Difficulty d)
{
    switch (d) {
    case Difficulty::Easy: return {130, 230, 110, 255};
    case Difficulty::Medium: return {255, 190, 70, 255};
    case Difficulty::Hard: return {255, 96, 80, 255};
    }
    return WHITE;
}

std::string formatThousands(long long value)
{
    const bool negative = value < 0;
    std::string digits = std::to_string(negative ? -value : value);
    std::string out;
    int count = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (count && count % 3 == 0) out.insert(out.begin(), ',');
        out.insert(out.begin(), *it);
        count++;
    }
    return negative ? "-" + out : out;
}

void Hud::reset()
{
    banners_.clear();
    shout_ = {};
    coinsShown_ = 0.0f;
    coinPulse_ = 0.0f;
    rpmNeedle_ = 0.0f;
    speedNeedle_ = 0.0f;
    time_ = 0.0f;
}

void Hud::shout(const std::string& text, Color color, float seconds)
{
    shout_ = {text, "", color, seconds, seconds};
}

void Hud::update(float dt, const Session& session)
{
    time_ += dt;
    const float target = static_cast<float>(session.coins());
    if (coinsShown_ < target) {
        coinsShown_ = std::fmin(target, coinsShown_ + std::fmax(40.0f, (target - coinsShown_) * 6.0f) * dt);
    }
    coinPulse_ = std::fmax(0.0f, coinPulse_ - dt * 3.0f);
    rpmNeedle_ = approach(rpmNeedle_, session.engineRpm(), 10.0f, dt);
    speedNeedle_ = approach(speedNeedle_, session.vehicle().speed() * 3.6f, 6.0f, dt);

    for (Banner& b : banners_) b.life -= dt;
    while (!banners_.empty() && banners_.front().life <= 0.0f) banners_.pop_front();
    shout_.life -= dt;
}

void Hud::onEvent(const GameEvent& e, const Session& session)
{
    (void)session;
    auto banner = [&](const std::string& title, long long value, Color color) {
        banners_.push_back({title, value > 0 ? "+" + formatThousands(value) : "", color, 2.2f, 2.2f});
        while (banners_.size() > 3) banners_.pop_front();
    };
    switch (e.type) {
    case EventType::Coin: coinPulse_ = 1.0f; break;
    case EventType::AirTime: banner(airTimeLabel(e.count), e.value, {120, 210, 255, 255}); break;
    case EventType::Flip: banner(e.back ? "BACK FLIP" : "FLIP", e.value, {255, 214, 60, 255}); break;
    case EventType::NeckFlip: banner("NECK FLIP", e.value, {255, 120, 90, 255}); break;
    case EventType::LevelUp: banner("LEVEL " + std::to_string(e.count) + " COMPLETE", e.value, {140, 240, 120, 255}); break;
    case EventType::NewRecord: banner("NEW RECORD!", 0, {255, 214, 60, 255}); break;
    case EventType::Wheelie: banner(wheelieLabel(e.count), e.value, {150, 245, 110, 255}); break;
    case EventType::CleanLanding: banner("CLEAN LANDING", e.value, {100, 235, 255, 255}); break;
    case EventType::LuckyFuel:
        banner("LUCKY FUEL!", e.value, {255, 220, 60, 255});
        shout("LUCKY FUEL!", {255, 224, 70, 255}, 1.8f);
        break;
    case EventType::RollCageHit:
        banner("CAGE HIT (" + std::to_string(e.count) + " LEFT)", 0, {255, 175, 70, 255});
        break;
    case EventType::RollCageBreak: shout("ROLL CAGE BROKEN!", {255, 110, 60, 255}, 2.0f); break;
    default: break;
    }
}

Rectangle Hud::gasPedal(const Ui& ui) { return ui.anchored(1.0f, 1.0f, -24.0f, -20.0f, 132.0f, 150.0f); }
Rectangle Hud::brakePedal(const Ui& ui) { return ui.anchored(0.0f, 1.0f, 24.0f, -20.0f, 132.0f, 110.0f); }
Rectangle Hud::specialPedal(const Ui& ui) { return ui.anchored(1.0f, 1.0f, -176.0f, -20.0f, 112.0f, 96.0f); }
Rectangle Hud::pauseButton(const Ui& ui) { return ui.anchored(1.0f, 0.0f, -20.0f, 18.0f, 60.0f, 60.0f); }

void Hud::draw(const Session& session, Ui& ui, bool gasDown, bool brakeDown, bool showPedals, bool specialDown) const
{
    drawFuel(session, ui);
    drawCoins(ui);
    drawNitro(session, ui);
    drawRollCage(session, ui);
    drawDistance(session, ui);
    drawGauges(session, ui);
    if (showPedals) {
        drawPedal(ui, brakePedal(ui), false, brakeDown);
        drawPedal(ui, gasPedal(ui), true, gasDown);
        if (session.hasThrusters() || session.hasNitro()) {
            drawSpecialPedal(ui, specialPedal(ui), session, specialDown);
        }
    }

    // Pause button.
    const Rectangle p = pauseButton(ui);
    const Vector2 c{p.x + p.width * 0.5f, p.y + p.height * 0.5f};
    DrawCircleV(c, p.width * 0.5f + ui.px(3), kInk);
    DrawCircleV(c, p.width * 0.5f, ui.hovered(p) ? Color{255, 210, 70, 255} : Color{252, 186, 40, 255});
    DrawRectangleRounded({c.x - ui.px(11), c.y - ui.px(13), ui.px(8), ui.px(26)}, 0.4f, 4, WHITE);
    DrawRectangleRounded({c.x + ui.px(3), c.y - ui.px(13), ui.px(8), ui.px(26)}, 0.4f, 4, WHITE);

    // Live air-time counter.
    if (session.airborne() && session.state() != RunState::Crashed) {
        const int bonus = airTimeBonus(session.airTime());
        if (bonus > 0) {
            char line[64];
            std::snprintf(line, sizeof(line), "%s  +%s", airTimeLabel(airTimeStep(session.airTime())),
                          formatThousands(bonus).c_str());
            ui.text(line, {ui.width() * 0.5f, ui.px(118)}, ui.px(34), {150, 220, 255, 255}, Align::Center);
        }
    }
    // Live wheelie counter.
    if (session.wheeling() && session.state() != RunState::Crashed) {
        const int bonus = wheelieBonus(session.wheelieTime());
        if (bonus > 0) {
            char line[64];
            std::snprintf(line, sizeof(line), "%s  +%s", wheelieLabel(wheelieStep(session.wheelieTime())),
                          formatThousands(bonus).c_str());
            ui.text(line, {ui.width() * 0.5f, ui.px(118)}, ui.px(34), {150, 245, 110, 255}, Align::Center);
        }
    }
    drawBanners(ui);

    if (shout_.life > 0.0f) {
        const float t = shout_.life / shout_.maxLife;
        const float pop = t > 0.85f ? 1.0f + (t - 0.85f) * 2.0f : 1.0f;
        Color color = shout_.color;
        color.a = static_cast<unsigned char>(255 * clampf(t * 4.0f, 0.0f, 1.0f));
        ui.text(shout_.title.c_str(), {ui.width() * 0.5f, ui.height() * 0.32f}, ui.px(72) * pop, color, Align::Center);
    }
}

void Hud::drawFuel(const Session& session, Ui& ui) const
{
    const float s = ui.scale();
    const Vector2 icon{ui.px(40), ui.px(44)};
    drawJerrycanIcon(icon, s);
    const float fuel = session.fuel();
    const Color fill = fuel > 0.5f ? draw::mix({250, 210, 50, 255}, {110, 210, 60, 255}, (fuel - 0.5f) * 2.0f)
                                   : draw::mix({230, 50, 40, 255}, {250, 210, 50, 255}, fuel * 2.0f);
    const Rectangle bar{ui.px(66), ui.px(32), ui.px(230), ui.px(24)};
    drawOutlinedBar(bar, fuel, fill, s);
    if (fuel <= Session::kLowFuel && session.state() == RunState::Running) {
        const bool on = std::fmod(time_, 0.6f) < 0.35f;
        if (on) ui.text("FUEL LOW", {bar.x + bar.width + ui.px(14), bar.y - ui.px(4)}, ui.px(30), {255, 80, 60, 255});
    }
}

void Hud::drawCoins(Ui& ui) const
{
    const float pulse = 1.0f + coinPulse_ * 0.25f;
    drawCoinIcon({ui.px(40), ui.px(92)}, ui.px(15) * pulse);
    const std::string text = formatThousands(static_cast<long long>(coinsShown_));
    ui.text(text.c_str(), {ui.px(66), ui.px(74)}, ui.px(36), {255, 230, 120, 255});
}

void Hud::drawNitro(const Session& session, Ui& ui) const
{
    if (!session.hasNitro()) return;
    const float s = ui.scale();
    const float nitro = session.nitro();
    const bool bright = session.boosting() || nitro >= 0.99f;
    const float pulse = bright ? 0.5f + 0.5f * std::sin(time_ * 8.0f) : 0.0f;
    const Color fill = draw::mix({60, 210, 255, 255}, {140, 245, 255, 255}, session.boosting() ? 1.0f : pulse);
    drawNitroBadge({ui.px(40), ui.px(133)}, ui.px(14), nitro >= 0.02f ? fill : Color{74, 84, 102, 255});
    const Rectangle bar{ui.px(66), ui.px(122), ui.px(230), ui.px(22)};
    drawOutlinedBar(bar, nitro, fill, s);
}

void Hud::drawRollCage(const Session& session, Ui& ui) const
{
    if (!session.vehicle().hasRollCage()) return;
    const float s = ui.scale();
    const float y = session.hasNitro() ? ui.px(156) : ui.px(122);
    const int hitsLeft = session.vehicle().rollCageHitsLeft();
    const int maxHits = std::max(1, session.vehicle().rollCageMaxHits());

    Color badgeCol{205, 86, 76, 255};
    if (hitsLeft > 0) {
        const float ratio = static_cast<float>(hitsLeft) / static_cast<float>(maxHits);
        badgeCol = ratio >= 0.75f ? Color{120, 195, 240, 255}
                                  : (ratio >= 0.4f ? Color{255, 190, 65, 255} : Color{255, 96, 80, 255});
    }
    drawCageBadge({ui.px(40), y + ui.px(11)}, s, badgeCol);

    if (hitsLeft == 0) {
        ui.text("CAGE BROKEN", {ui.px(66), y}, ui.px(21), {220, 95, 85, 235});
        return;
    }

    ui.text("CAGE", {ui.px(66), y + ui.px(1)}, ui.px(19), {220, 230, 245, 255});
    const float startX = ui.px(124);
    const float pipW = ui.px(22);
    const float pipH = ui.px(16);
    const float gap = ui.px(6);
    for (int i = 0; i < maxHits; ++i) {
        const Rectangle pip{startX + static_cast<float>(i) * (pipW + gap), y + ui.px(3), pipW, pipH};
        DrawRectangleRounded({pip.x - 2 * s, pip.y - 2 * s, pip.width + 4 * s, pip.height + 4 * s}, 0.35f, 4, kInk);
        DrawRectangleRounded(pip, 0.35f, 4, i < hitsLeft ? badgeCol : Color{58, 58, 70, 255});
        if (i < hitsLeft) {
            DrawRectangleRounded({pip.x + 2 * s, pip.y + 2 * s, pip.width - 4 * s, pip.height * 0.35f}, 0.35f, 4,
                                 draw::withAlpha(WHITE, 0.35f));
        }
    }
}

void Hud::drawDistance(const Session& session, Ui& ui) const
{
    char line[96];
    std::snprintf(line, sizeof(line), "%dm", static_cast<int>(session.distance()));
    ui.text(line, {ui.width() * 0.5f, ui.px(14)}, ui.px(56), WHITE, Align::Center);

    const int nextLevel = session.levelsCompleted() + 1;
    if (!goalLine_.empty()) {
        std::snprintf(line, sizeof(line), "%s", goalLine_.c_str());
    } else if (session.bestDistance() > 1.0f) {
        std::snprintf(line, sizeof(line), "LEVEL %d: %dm      BEST %dm", nextLevel,
                      static_cast<int>(session.nextLevelDistance()), static_cast<int>(session.bestDistance()));
    } else {
        std::snprintf(line, sizeof(line), "LEVEL %d: %dm", nextLevel, static_cast<int>(session.nextLevelDistance()));
    }
    ui.text(line, {ui.width() * 0.5f, ui.px(72)}, ui.px(24), {230, 236, 255, 255}, Align::Center);
    const Difficulty d = session.stage().difficulty;
    if (d != Difficulty::Easy) {
        ui.text(difficultyDef(d).name, {ui.width() * 0.5f, ui.px(100)}, ui.px(22), difficultyColor(d), Align::Center);
    }
}

void Hud::drawGauges(const Session& session, Ui& ui) const
{
    (void)session;
    const float r = ui.px(56);
    const float y = ui.height() - ui.px(72);
    char reading[32];
    std::snprintf(reading, sizeof(reading), "%d", static_cast<int>(rpmNeedle_ * 8000.0f / 100.0f) * 100);
    drawDial({ui.width() * 0.5f - ui.px(70), y}, r, rpmNeedle_, "RPM", reading, ui, true);
    std::snprintf(reading, sizeof(reading), "%d", static_cast<int>(speedNeedle_));
    drawDial({ui.width() * 0.5f + ui.px(70), y}, r, speedNeedle_ / 160.0f, "KM/H", reading, ui, false);
}

void Hud::drawPedal(Ui& ui, Rectangle r, bool gas, bool down) const
{
    const float s = ui.scale();
    Rectangle p = r;
    if (down) {
        // Pressed pedals sink and tilt away a little.
        p.y += ui.px(12);
        p.height -= ui.px(12);
        p.x += ui.px(4);
        p.width -= ui.px(8);
    }
    DrawRectangleRounded({p.x - 4 * s, p.y - 4 * s + ui.px(6), p.width + 8 * s, p.height + 8 * s}, 0.25f, 8,
                         draw::withAlpha(BLACK, 0.35f));
    DrawRectangleRounded({p.x - 4 * s, p.y - 4 * s, p.width + 8 * s, p.height + 8 * s}, 0.25f, 8,
                         down ? Color{255, 214, 60, 255} : kInk);
    const Color face = down ? Color{132, 136, 150, 255} : Color{196, 200, 212, 255};
    DrawRectangleRounded(p, 0.25f, 8, face);
    // Grip grooves.
    const int grooves = gas ? 6 : 4;
    for (int i = 0; i < grooves; ++i) {
        const float gy = p.y + p.height * (0.18f + 0.64f * static_cast<float>(i) / static_cast<float>(grooves - 1));
        DrawRectangleRounded({p.x + p.width * 0.16f, gy - 3 * s, p.width * 0.68f, 6 * s}, 1.0f, 4, draw::shade(face, 0.72f));
    }
    ui.text(gas ? "GAS" : "BRAKE", {p.x + p.width * 0.5f, p.y - ui.px(32)}, ui.px(24),
            down ? Color{255, 230, 120, 255} : WHITE, Align::Center);
    (void)kPanel;
}

void Hud::drawSpecialPedal(Ui& ui, Rectangle r, const Session& session, bool down) const
{
    const float s = ui.scale();
    const bool thruster = session.hasThrusters();
    const float nitro = session.nitro();
    const bool empty = !thruster && nitro < 0.02f;
    const bool active = thruster ? (down || session.thrusting()) : ((down && !empty) || session.boosting());

    Rectangle p = r;
    if (down && !empty) {
        p.y += ui.px(8);
        p.height -= ui.px(8);
        p.x += ui.px(3);
        p.width -= ui.px(6);
    }

    if (active) {
        const Color glow = thruster ? draw::withAlpha({255, 165, 45, 255}, 0.42f)
                                    : draw::withAlpha({60, 225, 255, 255}, 0.48f);
        DrawRectangleRounded({p.x - 8 * s, p.y - 8 * s, p.width + 16 * s, p.height + 16 * s}, 0.3f, 8, glow);
    }

    DrawRectangleRounded({p.x - 4 * s, p.y - 4 * s + ui.px(6), p.width + 8 * s, p.height + 8 * s}, 0.25f, 8,
                         draw::withAlpha(BLACK, 0.35f));

    Color border = kInk;
    if (thruster) {
        if (active) border = {255, 210, 80, 255};
    } else if (!empty) {
        border = active ? Color{150, 248, 255, 255} : Color{75, 210, 255, 255};
    }
    DrawRectangleRounded({p.x - 4 * s, p.y - 4 * s, p.width + 8 * s, p.height + 8 * s}, 0.25f, 8, border);

    if (thruster) {
        const Color face = active ? Color{250, 162, 42, 255} : Color{208, 128, 52, 255};
        DrawRectangleRounded(p, 0.25f, 8, face);
        DrawRectangleRounded({p.x + 6 * s, p.y + 4 * s, p.width - 12 * s, p.height * 0.32f}, 0.5f, 6,
                             draw::withAlpha(WHITE, active ? 0.32f : 0.2f));
    } else {
        const Color base = empty ? Color{78, 84, 98, 230} : Color{42, 64, 92, 245};
        DrawRectangleRounded(p, 0.25f, 8, base);
        if (!empty) {
            const Rectangle inner{p.x + 4 * s, p.y + 4 * s, p.width - 8 * s, p.height - 8 * s};
            const float fillH = clampf(inner.height * nitro, 12.0f * s, inner.height);
            const Rectangle fillRec{inner.x, inner.y + inner.height - fillH, inner.width, fillH};
            const Color fillCol = active ? Color{110, 240, 255, 255} : Color{45, 192, 248, 235};
            DrawRectangleRounded(fillRec, 0.22f, 8, fillCol);
            DrawRectangleRounded({inner.x + 3 * s, inner.y + 2 * s, inner.width - 6 * s, inner.height * 0.28f}, 0.5f,
                                 6, draw::withAlpha(WHITE, active ? 0.34f : 0.18f));
        }
    }

    const Color titleCol = empty ? Color{170, 178, 192, 210}
                                 : (active ? WHITE : (thruster ? Color{255, 236, 175, 255} : Color{215, 248, 255, 255}));
    const Color subCol = empty ? Color{140, 146, 160, 190} : Color{230, 238, 250, 225};
    ui.text(thruster ? "THRUST" : "NITRO", {p.x + p.width * 0.5f, p.y + p.height * 0.22f}, ui.px(22), titleCol,
            Align::Center);
    ui.text("SPACE", {p.x + p.width * 0.5f, p.y + p.height * 0.58f}, ui.px(16), subCol, Align::Center);
}

void Hud::drawBanners(Ui& ui) const
{
    float y = ui.px(160);
    for (const Banner& b : banners_) {
        const float t = b.life / b.maxLife;
        const float pop = t > 0.88f ? 1.0f + (t - 0.88f) * 2.5f : 1.0f;
        Color c = b.color;
        c.a = static_cast<unsigned char>(255 * clampf(t * 4.0f, 0.0f, 1.0f));
        Color white = WHITE;
        white.a = c.a;
        ui.text(b.title.c_str(), {ui.width() * 0.5f, y}, ui.px(40) * pop, c, Align::Center);
        if (!b.value.empty()) ui.text(b.value.c_str(), {ui.width() * 0.5f, y + ui.px(42)}, ui.px(34) * pop, white, Align::Center);
        y += ui.px(b.value.empty() ? 52 : 92);
    }
}

} // namespace hr
