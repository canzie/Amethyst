/*
 * Input event types and structures for UI interaction
 */

#ifndef AMETHYST__INPUT_EVENTS_H
#define AMETHYST__INPUT_EVENTS_H

#include "math/math.h"
#include <functional>
#include <variant>

namespace Amethyst {

enum MouseButton {
    MOUSE_BUTTON_1 = 0,
    MOUSE_BUTTON_2 = 1,
    MOUSE_BUTTON_3 = 2,
    MOUSE_BUTTON_4 = 3,
    MOUSE_BUTTON_5 = 4,
    MOUSE_BUTTON_6 = 5,
    MOUSE_BUTTON_7 = 6,
    MOUSE_BUTTON_8 = 7
};

enum MouseAction {
    MOUSE_ACTION_RELEASE = 0,
    MOUSE_ACTION_PRESS = 1,
    MOUSE_ACTION_REPEAT = 2
};

enum KeyAction {
    KEY_ACTION_RELEASE = 0,
    KEY_ACTION_PRESS = 1,
    KEY_ACTION_REPEAT = 2
};

enum KeyCode {
    KEY_BACKSPACE = 259,
    KEY_DELETE = 261,
    KEY_LEFT = 263,
    KEY_RIGHT = 262,
    KEY_UP = 265,
    KEY_DOWN = 264,
    KEY_HOME = 268,
    KEY_END = 269,
    KEY_ENTER = 257,
    KEY_ESCAPE = 256,
    KEY_TAB = 258,
    KEY_A = 65,
    KEY_C = 67,
    KEY_V = 86,
    KEY_X = 88,
};

enum KeyModifier {
    MOD_SHIFT = 0x0001,
    MOD_CONTROL = 0x0002,
    MOD_ALT = 0x0004,
    MOD_SUPER = 0x0008,
    MOD_DOUBLE_CLICK = 0x0010,
};

struct MouseEvent {
    vec2 position;
    MouseButton button;
    MouseAction action;
    int mods;
};

struct KeyEvent {
    int key;
    int scancode;
    KeyAction action;
    int mods;
};

struct CursorMoveEvent {
    vec2 position;
    vec2 delta;
};

struct ScrollEvent {
    vec2 position;
    vec2 offset;
    int mods;
};

using InputEvent = std::variant<MouseEvent, KeyEvent, CursorMoveEvent, ScrollEvent>;

class Instance;

struct ClickEvent {
    vec2 position;
    MouseButton button;
    Instance *target;
};

using ClickCallback = std::function<void(const ClickEvent &)>;

enum InteractionCategory {
    INTERACTION_CATEGORY_NONE   = 0,
    INTERACTION_CATEGORY_HOVER  = 1 << 0,
    INTERACTION_CATEGORY_CLICK  = 1 << 1,
    INTERACTION_CATEGORY_SCROLL = 1 << 2,
    INTERACTION_CATEGORY_MOVE   = 1 << 3,
    INTERACTION_CATEGORY_KEY    = 1 << 4,
    INTERACTION_CATEGORY_ALL    = (1 << 8) - 1,
};

enum class InputType {
    NONE,
    MOUSE_BUTTON_1,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_MOVEMENT,
    MOUSE_WHEEL,
    KEYBOARD
};

enum class InputState {
    NONE,
    BEGIN,
    CHANGE,
    END,
    CANCEL
};

// position/delta are vec3 so the z component can carry the scroll-wheel amount.
struct InputObject {
    InputType type = InputType::NONE;
    InputState state = InputState::NONE;
    vec3 position{0.0f, 0.0f, 0.0f};
    vec3 delta{0.0f, 0.0f, 0.0f};
    int keyCode = 0;
    int modifiers = 0; // bitmask of KeyModifier (MOD_SHIFT/MOD_CONTROL/MOD_ALT/MOD_SUPER/MOD_DOUBLE_CLICK)
};

} // namespace Amethyst

#endif // AMETHYST__INPUT_EVENTS_H
