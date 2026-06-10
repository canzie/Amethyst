/*
 * Input interface for backend integration
 */

#ifndef AMETHYST__INPUT_INTERFACE_H
#define AMETHYST__INPUT_INTERFACE_H

#include "components/input_events.h"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

class Window;

enum CursorShape {
    CURSOR_ARROW,
    CURSOR_IBEAM,
    CURSOR_HAND,
    CURSOR_HORI_RESIZE,
    CURSOR_VERT_RESIZE,
    CURSOR_NWSE_RESIZE,
    CURSOR_NESW_RESIZE,
    CURSOR_ALL_RESIZE,
    CURSOR_NOT_ALLOWED,
    CURSOR_CROSSHAIR,
    CURSOR_COUNT
};

class InputInterface {
  public:
    static void registerWindow(Window *window);
    static void unregisterWindow(Window *window);

    static void setMousePosition(uint32_t x, uint32_t y);
    static void onMouseButton(int button, int action, int mods);
    static void onMouseScroll(float xoffset, float yoffset);
    static void onKey(int key, int scancode, int action, int mods);
    static void onChar(uint32_t codepoint);

    static void setCursorShape(CursorShape shape);
    static void setClipboardText(const std::string &text);
    static std::string getClipboardText();

    static bool pollKeyEvent(KeyEvent &outEvent);
    static bool pollCharEvent(uint32_t &outCodepoint);
    static void clearKeyEvents();

    // WIP stopgap: caches the last mods bitmask seen by any input handler until modifiers
    // are threaded through dispatch properly and InputObject can carry them end-to-end.
    static int getModifiers();

  public:
    static std::function<void(CursorShape)> onCursorShapeChanged;
    static std::function<void(const std::string &)> onSetClipboardText;
    static std::function<std::string()> onGetClipboardText;

  private:
    static constexpr size_t KEY_BUFFER_SIZE = 256;
    static constexpr size_t CHAR_BUFFER_SIZE = 256;

    static inline std::vector<Window *> s_windows;
    static inline uint32_t s_mouseX = 0;
    static inline uint32_t s_mouseY = 0;
    static inline int s_modifiers = 0;
    static inline CursorShape s_currentCursorShape = CursorShape::CURSOR_ARROW;
    static inline bool isWindowFocussed = false;

    static inline std::array<KeyEvent, KEY_BUFFER_SIZE> s_keyBuffer;
    static inline size_t s_keyBufferHead = 0;
    static inline size_t s_keyBufferTail = 0;

    static inline std::array<uint32_t, CHAR_BUFFER_SIZE> s_charBuffer;
    static inline size_t s_charBufferHead = 0;
    static inline size_t s_charBufferTail = 0;
};

} // namespace Amethyst

#endif // AMETHYST__INPUT_INTERFACE_H
