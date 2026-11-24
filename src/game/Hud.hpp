#pragma once

#include "game/Assets.hpp"
#include "game/Ui.hpp"
#include "sim/Session.hpp"

#include <raylib.h>

#include <deque>
#include <string>

namespace hr {

// In-run heads-up display: fuel, coins, distance, gauges, pedals and the
// banners that announce bonuses.
class Hud {
public:
    void reset();
    void update(float dt, const Session& session);
    void onEvent(const GameEvent& event, const Session& session);
    void draw(const Session& session, Ui& ui, bool gasDown, bool brakeDown, bool showPedals,
              bool specialDown = false) const;

    // Screen rectangles of the touch/mouse controls (depend on window size).
    static Rectangle gasPedal(const Ui& ui);
    static Rectangle brakePedal(const Ui& ui);
    static Rectangle specialPedal(const Ui& ui);
    static Rectangle pauseButton(const Ui& ui);

    // Big centred message (e.g. "DRIVER DOWN!").
    void shout(const std::string& text, Color color, float seconds);
    // Replaces the level / best line under the distance (online races show the goal).
    void setGoalLine(std::string line) { goalLine_ = std::move(line); }

private:
    struct Banner {
        std::string title;
        std::string value;
        Color color;
        float life;
        float maxLife;
    };

    void drawFuel(const Session& session, Ui& ui) const;
    void drawCoins(Ui& ui) const;
    void drawNitro(const Session& session, Ui& ui) const;
    void drawRollCage(const Session& session, Ui& ui) const;
    void drawDistance(const Session& session, Ui& ui) const;
    void drawGauges(const Session& session, Ui& ui) const;
    void drawPedal(Ui& ui, Rectangle r, bool gas, bool down) const;
    void drawSpecialPedal(Ui& ui, Rectangle r, const Session& session, bool down) const;
    void drawBanners(Ui& ui) const;

    std::deque<Banner> banners_;
    Banner shout_{};
    std::string goalLine_;
    float coinsShown_ = 0.0f;
    float coinPulse_ = 0.0f;
    float rpmNeedle_ = 0.0f;
    float speedNeedle_ = 0.0f;
    float time_ = 0.0f;
};

// "12,345"
std::string formatThousands(long long value);

// Badge colour for a difficulty: green, orange, red.
Color difficultyColor(Difficulty d);

} // namespace hr
