/*
 * Input event types and structures for UI interaction
 */

#ifndef AMETHYST__INPUT_EVENTS_H
#define AMETHYST__INPUT_EVENTS_H

#include <functional>
#include <glm/glm.hpp>
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
};

struct MouseEvent {
    glm::vec2 position;
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
    glm::vec2 position;
    glm::vec2 delta;
};

struct ScrollEvent {
    glm::vec2 position;
    glm::vec2 offset;
    int mods;
};

using InputEvent = std::variant<MouseEvent, KeyEvent, CursorMoveEvent, ScrollEvent>;

class Instance;

struct ClickEvent {
    glm::vec2 position;
    MouseButton button;
    Instance *target;
};

using ClickCallback = std::function<void(const ClickEvent &)>;

} // namespace Amethyst

#endif // AMETHYST__INPUT_EVENTS_H
