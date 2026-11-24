#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>

namespace hr {

enum class Action {
    Gas,
    Brake,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Pause,
    Restart,
    Fullscreen,
    Screenshot,
    Debug,
    Count
};

// Collects keyboard, mouse and gamepad state once per frame. Automation can
// inject virtual presses that go through exactly the same paths as real ones.
class Input {
public:
    void update();

    bool down(Action a) const { return cur_[idx(a)]; }
    bool pressed(Action a) const { return cur_[idx(a)] && !prev_[idx(a)]; }
    bool released(Action a) const { return !cur_[idx(a)] && prev_[idx(a)]; }

    // Analogue pedals in [0, 1] (keyboard gives 0 or 1, triggers anything between).
    float gas() const { return gas_; }
    float brake() const { return brake_; }

    Vector2 mouse() const { return mouse_; }
    bool mouseDown() const { return mouseDown_; }
    bool mousePressed() const { return mouseDown_ && !mousePrevDown_; }
    bool mouseReleased() const { return !mouseDown_ && mousePrevDown_; }
    bool mouseMoved() const { return mouseMoved_; }
    // True when the last menu interaction came from keys/gamepad rather than the mouse.
    bool usingKeys() const { return usingKeys_; }

    // Text entry this frame, for text fields: printable characters typed,
    // Backspace presses (with key repeat), Enter and Escape.
    const std::string& typed() const { return typed_; }
    int backspaces() const { return backspaces_; }
    bool enterPressed() const { return enter_; }
    bool escapePressed() const { return escape_; }

    // --- automation -------------------------------------------------------
    void setVirtual(Action a, bool isDown) { virtual_[idx(a)] = isDown; }
    // Typed on the next frame (a virtual "confirm" press counts as Enter).
    void injectText(const std::string& text) { injectedText_ += text; }
    void injectBackspaces(int count) { injectedBackspaces_ += count; }
    void setVirtualMouse(Vector2 pos, bool isDown)
    {
        virtualMouse_ = pos;
        virtualMouseDown_ = isDown;
        virtualMouseActive_ = true;
    }
    // When exclusive, real devices are ignored (deterministic scripted runs).
    void setExclusiveVirtual(bool on) { exclusive_ = on; }

private:
    static std::size_t idx(Action a) { return static_cast<std::size_t>(a); }

    static constexpr std::size_t kCount = static_cast<std::size_t>(Action::Count);
    std::array<bool, kCount> cur_{};
    std::array<bool, kCount> prev_{};
    std::array<bool, kCount> virtual_{};
    float gas_ = 0.0f;
    float brake_ = 0.0f;
    Vector2 mouse_{};
    Vector2 lastRealMouse_{};
    bool mouseDown_ = false;
    bool mousePrevDown_ = false;
    bool mouseMoved_ = false;
    bool usingKeys_ = false;
    Vector2 virtualMouse_{};
    bool virtualMouseDown_ = false;
    bool virtualMouseActive_ = false;
    bool exclusive_ = false;
    std::string typed_;
    int backspaces_ = 0;
    bool enter_ = false;
    bool escape_ = false;
    std::string injectedText_;
    int injectedBackspaces_ = 0;
};

} // namespace hr
