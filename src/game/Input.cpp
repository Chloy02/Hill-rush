#include "game/Input.hpp"

#include <cmath>

namespace hr {

namespace {

constexpr int kPad = 0;

bool anyKey(std::initializer_list<int> keys)
{
    for (int k : keys) {
        if (IsKeyDown(k)) return true;
    }
    return false;
}

bool padButton(int button) { return IsGamepadAvailable(kPad) && IsGamepadButtonDown(kPad, button); }

float padTrigger(int axis)
{
    if (!IsGamepadAvailable(kPad)) return 0.0f;
    // Triggers rest at -1 and go to +1 when fully pressed.
    const float v = GetGamepadAxisMovement(kPad, axis);
    const float t = (v + 1.0f) * 0.5f;
    return t > 0.08f ? t : 0.0f;
}

float padStickX()
{
    if (!IsGamepadAvailable(kPad)) return 0.0f;
    const float v = GetGamepadAxisMovement(kPad, GAMEPAD_AXIS_LEFT_X);
    return std::fabs(v) > 0.35f ? v : 0.0f;
}

float padStickY()
{
    if (!IsGamepadAvailable(kPad)) return 0.0f;
    const float v = GetGamepadAxisMovement(kPad, GAMEPAD_AXIS_LEFT_Y);
    return std::fabs(v) > 0.5f ? v : 0.0f;
}

} // namespace

void Input::update()
{
    prev_ = cur_;
    mousePrevDown_ = mouseDown_;
    std::array<bool, kCount> real{};
    float realGas = 0.0f;
    float realBrake = 0.0f;

    if (!exclusive_) {
        const float stickX = padStickX();
        const float stickY = padStickY();
        real[idx(Action::Gas)] = anyKey({KEY_RIGHT, KEY_D}) || padButton(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) ||
                                 padButton(GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        real[idx(Action::Brake)] = anyKey({KEY_LEFT, KEY_A}) || padButton(GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
                                   padButton(GAMEPAD_BUTTON_LEFT_TRIGGER_1);
        real[idx(Action::Up)] = anyKey({KEY_UP, KEY_W}) || padButton(GAMEPAD_BUTTON_LEFT_FACE_UP) || stickY < 0.0f;
        real[idx(Action::Down)] = anyKey({KEY_DOWN, KEY_S}) || padButton(GAMEPAD_BUTTON_LEFT_FACE_DOWN) || stickY > 0.0f;
        real[idx(Action::Left)] = anyKey({KEY_LEFT, KEY_A}) || padButton(GAMEPAD_BUTTON_LEFT_FACE_LEFT) || stickX < 0.0f;
        real[idx(Action::Right)] = anyKey({KEY_RIGHT, KEY_D}) || padButton(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || stickX > 0.0f;
        // Alt+Enter is the fullscreen shortcut, so it must not also press the focused button.
        const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        real[idx(Action::Confirm)] = (!alt && anyKey({KEY_ENTER, KEY_KP_ENTER})) || IsKeyDown(KEY_SPACE) ||
                                     padButton(GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
        real[idx(Action::Back)] = anyKey({KEY_ESCAPE, KEY_BACKSPACE}) || padButton(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
        real[idx(Action::Pause)] = anyKey({KEY_ESCAPE, KEY_P}) || padButton(GAMEPAD_BUTTON_MIDDLE_RIGHT);
        real[idx(Action::Restart)] = IsKeyDown(KEY_R) || padButton(GAMEPAD_BUTTON_MIDDLE_LEFT);
        real[idx(Action::Fullscreen)] =
            IsKeyDown(KEY_F11) || ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && IsKeyDown(KEY_ENTER));
        real[idx(Action::Screenshot)] = IsKeyDown(KEY_F12);
        real[idx(Action::Debug)] = IsKeyDown(KEY_F3);

        realGas = real[idx(Action::Gas)] ? 1.0f : 0.0f;
        realBrake = real[idx(Action::Brake)] ? 1.0f : 0.0f;
        realGas = std::fmax(realGas, padTrigger(GAMEPAD_AXIS_RIGHT_TRIGGER));
        realBrake = std::fmax(realBrake, padTrigger(GAMEPAD_AXIS_LEFT_TRIGGER));
        if (realGas > 0.0f) real[idx(Action::Gas)] = true;
        if (realBrake > 0.0f) real[idx(Action::Brake)] = true;
    }

    for (size_t i = 0; i < kCount; ++i) cur_[i] = real[i] || virtual_[i];
    gas_ = std::fmax(realGas, virtual_[idx(Action::Gas)] ? 1.0f : 0.0f);
    brake_ = std::fmax(realBrake, virtual_[idx(Action::Brake)] ? 1.0f : 0.0f);

    // Mouse (touch screens report as a mouse too).
    const Vector2 realMouse = GetMousePosition();
    mouseMoved_ = false;
    if (!exclusive_ && (realMouse.x != lastRealMouse_.x || realMouse.y != lastRealMouse_.y)) {
        mouseMoved_ = true;
        mouse_ = realMouse;
        virtualMouseActive_ = false;
    }
    lastRealMouse_ = realMouse;
    bool realDown = !exclusive_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (virtualMouseActive_) {
        if (mouse_.x != virtualMouse_.x || mouse_.y != virtualMouse_.y) mouseMoved_ = true;
        mouse_ = virtualMouse_;
        mouseDown_ = virtualMouseDown_ || realDown;
    } else {
        if (exclusive_) realDown = false;
        mouseDown_ = realDown;
    }

    for (Action a : {Action::Up, Action::Down, Action::Left, Action::Right, Action::Confirm}) {
        if (pressed(a)) usingKeys_ = true;
    }
    if (mouseMoved_ || mousePressed()) usingKeys_ = false;

    // Text entry. The character queue is always drained, so keys pressed
    // during a scripted run don't pile up.
    typed_.clear();
    for (int c = GetCharPressed(); c > 0; c = GetCharPressed()) {
        if (!exclusive_ && c >= 32 && c < 127) typed_ += static_cast<char>(c);
    }
    typed_ += injectedText_;
    injectedText_.clear();
    const bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    const bool realBackspace = IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE);
    backspaces_ = (!exclusive_ && realBackspace ? 1 : 0) + injectedBackspaces_;
    injectedBackspaces_ = 0;
    const bool realEnter = !alt && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER));
    const bool virtualConfirm = virtual_[idx(Action::Confirm)] && !prev_[idx(Action::Confirm)];
    enter_ = (!exclusive_ && realEnter) || virtualConfirm;
    escape_ = !exclusive_ && IsKeyPressed(KEY_ESCAPE);
}

} // namespace hr
