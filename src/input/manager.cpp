#include "manager.hpp"

// Input manager — Entergram emulator key mapping
//
// Maps keyboard/gamepad inputs to the Entergram 8-button layout:
//   Enter: Circle (confirm)
//   Esc / Space: Cross (cancel)
//   Z: Circle, X: Cross (for VN keyboard controls)
//   Arrow keys: Up/Down/Left/Right
//   Space: Circle (confirm)
//   C: Start, V: Select

namespace entergram {

InputManager::InputManager() = default;
InputManager::~InputManager() = default;

void InputManager::update() {
    previous_ = current_;
    // Platform-specific polling will be implemented with SDL2
    // For now, we just provide the state transition logic
}

bool InputManager::was_pressed(KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    if (idx >= 12) return false;
    return current_.keys[idx] && !previous_.keys[idx];
}

bool InputManager::is_held(KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    if (idx >= 12) return false;
    return current_.keys[idx];
}

bool InputManager::was_released(KeyCode key) const {
    size_t idx = static_cast<size_t>(key);
    if (idx >= 12) return false;
    return !current_.keys[idx] && previous_.keys[idx];
}

KeyCode InputManager::map_key(int platform_keycode) {
    // Map platform-specific keys to Entergram layout
    // These values correspond to common keyboard layouts for visual novels
    switch (platform_keycode) {
        // Primary confirm/cancel (keyboard)
        case 'Z':        return KeyCode::Circle;
        case 'z':        return KeyCode::Circle;
        case 'X':        return KeyCode::Cross;
        case 'x':        return KeyCode::Cross;
        case 32:          return KeyCode::Circle;  // Space
        case 13:          return KeyCode::Circle;  // Enter
        case 27:          return KeyCode::Cross;   // Escape

        // Action buttons
        case 'A':        return KeyCode::Square;
        case 'a':        return KeyCode::Square;
        case 'S':        return KeyCode::Triangle;
        case 's':        return KeyCode::Triangle;

        // Shoulder buttons
        case 'Q':        return KeyCode::L1;
        case 'q':        return KeyCode::L1;
        case 'E':        return KeyCode::R1;
        case 'e':        return KeyCode::R1;

        // System buttons
        case 'C':        return KeyCode::Start;
        case 'c':        return KeyCode::Start;
        case 'V':        return KeyCode::Select;
        case 'v':        return KeyCode::Select;

        // D-pad / arrow keys
        case 273:         return KeyCode::Up;    // Up arrow (SDLK_UP)
        case 274:         return KeyCode::Down;   // Down arrow
        case 276:         return KeyCode::Left;  // Left arrow
        case 275:         return KeyCode::Right; // Right arrow

        default:
            return KeyCode::Circle;  // Default to confirm
    }
}

} // namespace entergram
