#pragma once

#include "game/Assets.hpp"
#include "game/Input.hpp"

#include <raylib.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace hr {

enum class ButtonKind { Go, Normal, Back, Info, Buy };

// What a text field accepts: player names (upper case letters, digits,
// spaces, dashes) or network addresses.
enum class TextKind { Name, Address };

// Immediate-mode widgets. Layout is written for a 1280x720 "logical" screen
// and scaled to the real window; focus can move with keys/gamepad.
class Ui {
public:
    static constexpr float kLogicalW = 1280.0f;
    static constexpr float kLogicalH = 720.0f;

    void beginFrame(Input& input, const Assets& assets);
    void endFrame();

    std::function<void()> onClick;  // played when any widget activates

    float scale() const { return scale_; }
    float width() const { return width_; }
    float height() const { return height_; }
    float px(float v) const { return v * scale_; }

    // Rectangle given in logical units relative to the screen centre.
    Rectangle centered(float dx, float dy, float w, float h) const;
    // Rectangle anchored to a screen corner/edge: ax, ay in {0, 0.5, 1}.
    Rectangle anchored(float ax, float ay, float dx, float dy, float w, float h) const;

    bool button(const char* id, Rectangle r, const char* label, ButtonKind kind, bool enabled = true,
                float fontSize = 0.0f);
    // Arrow-shaped button pointing left (dir = -1) or right (dir = 1).
    bool arrowButton(const char* id, Rectangle r, int dir, bool enabled = true);
    // Returns the (possibly changed) value in [0, 1].
    float slider(const char* id, Rectangle r, float value);
    bool toggle(const char* id, Rectangle r, bool value);
    // One-line text box. Click it (or press Enter on it) to type; Enter, Esc
    // or a click elsewhere finishes. Returns true on the frame typing finishes.
    bool textField(const char* id, Rectangle r, std::string& value, size_t maxLength, TextKind kind,
                   const char* placeholder);
    // True while a text field has the keyboard: screens should ignore Back
    // (Esc / Backspace) and the like meanwhile.
    bool editing() const { return editing_ != 0; }

    void panel(Rectangle r, Color fill, Color border, float roundness = 0.18f);
    void text(const char* s, Vector2 pos, float size, Color color, Align align = Align::Left,
              bool outlined = true);

    // This frame, Left/Right won't move keyboard focus (carousels use them).
    void lockHorizontal() { lockHorizontal_ = true; }

    void setFocus(const char* id);
    void setDefaultFocus(const char* id) { defaultFocus_ = hashId(id); }
    bool focused(const char* id) const { return focus_ == hashId(id); }
    bool hovered(Rectangle r) const;

    const Assets& assets() const { return *assets_; }
    Input& input() { return *input_; }

    static uint64_t hashId(const char* id);

private:
    struct Widget {
        uint64_t id;
        Rectangle rect;
        bool horizontal;  // consumes left/right (sliders)
    };

    void registerWidget(uint64_t id, Rectangle r, bool horizontal);
    bool activated(uint64_t id, Rectangle r, bool enabled);
    void drawFocus(Rectangle r) const;

    Input* input_ = nullptr;
    const Assets* assets_ = nullptr;
    float scale_ = 1.0f;
    float width_ = kLogicalW;
    float height_ = kLogicalH;
    std::vector<Widget> widgets_;
    std::vector<Widget> lastWidgets_;
    uint64_t focus_ = 0;
    uint64_t defaultFocus_ = 0;
    uint64_t pressedId_ = 0;
    uint64_t editing_ = 0;  // text field being typed into
    bool consumeConfirm_ = false;
    bool lockHorizontal_ = false;
};

} // namespace hr
