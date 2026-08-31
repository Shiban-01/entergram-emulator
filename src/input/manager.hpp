#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace entergram {

// Entergram engine key codes
// Maps physical keys to the 8-button layout used by Entergram games:
//   Circle (confirm), Cross (cancel), Square, Triangle,
//   L1, R1, Start, Select
enum class KeyCode {
    Circle  = 0,  // Confirm (A in Entergram UI)
    Cross   = 1,  // Cancel (B in Entergram UI)
    Square  = 2,
    Triangle = 3,
    L1      = 4,
    R1      = 5,
    Start   = 6,
    Select  = 7,
    // Navigation
    Up      = 8,
    Down    = 9,
    Left    = 10,
    Right   = 11,
};

// Input state snapshot at a point in time
struct InputState {
    bool keys[12] = {false};  // 12 key slots
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    bool mouse_left = false;
    bool mouse_right = false;
};

// Input manager — handles keyboard/gamepad input mapping.
// Maps physical keys to the Entergram 8-button layout.
// This uses SDL2 for the actual input polling, but the interface
// is abstraction-agnostic for portability.
class InputManager {
public:
    InputManager();
    ~InputManager();

    // Update input state from platform events
    void update();

    // Get current input state
    const InputState& current_state() const { return current_; }

    // Check if a key was just pressed (transition from up to down)
    bool was_pressed(KeyCode key) const;

    // Check if a key is currently held
    bool is_held(KeyCode key) const;

    // Check if a key was just released
    bool was_released(KeyCode key) const;

    // Map a platform-specific key code to an Entergram KeyCode
    static KeyCode map_key(int platform_keycode);

private:
    InputState current_;
    InputState previous_;
};

} // namespace entergram
