#pragma once

#include "game/Ui.hpp"
#include "net/Protocol.hpp"

#include <raylib.h>

#include <cstdint>
#include <deque>
#include <string>

namespace hr {

// Short notices ("SAM JOINED") shown for a few seconds.
class Toasts {
public:
    // Moves new notices over (emptying `notices`).
    void take(std::deque<std::string>& notices);
    void update(float dt);
    // The newest notice at logical height `y`: centred, or from logical x `left` when given.
    void draw(Ui& ui, float y, float left = -1.0f) const;

private:
    struct Item {
        std::string text;
        float life;
    };
    std::deque<Item> items_;
};

// Centred text wrapped to `maxWidth` (logical units), from logical height `y`.
void drawWrapped(Ui& ui, const std::string& text, float y, float maxWidth, float size, Color color);

// A player's colour in online races (their room colour index).
Color playerColor(uint8_t index);

// Rounded pill with the cartoon outline.
void outlinedPill(Ui& ui, Rectangle r, Color fill);

// Name tag with a little pointer, centred above `anchor` (screen space).
void drawNameTag(Ui& ui, Vector2 anchor, const char* text, Color color, float size = 24.0f);

// Avatar: the player's initial in a circle of their colour.
void drawAvatar(Ui& ui, Vector2 centre, float radius, const std::string& name, Color color);

// A vehicle at rest, centred on `centre` (screen space), `zoom` logical pixels per metre.
void drawVehicleThumb(Ui& ui, const VehicleDef& def, Vector2 centre, float zoom, float time);

// "RACE TO 1,000m", or "FURTHEST WINS" for races without a finish.
std::string goalText(int goal);
// "1:02.4"
std::string raceTimeText(float seconds);
// "1ST", "2ND", "3RD", "4TH"
const char* placeText(int place);
// Gold, silver, bronze, then plain.
Color placeColor(int place);

} // namespace hr
