/*
 * Input interface implementation
 */

#include "components/input_interface.h"
#include "components/window.h"

namespace Amethyst {

std::function<void(CursorShape)> InputInterface::onCursorShapeChanged;
std::function<void(bool)> InputInterface::onCursorLockChanged;
std::function<void(const std::string &)> InputInterface::onSetClipboardText;
std::function<std::string()> InputInterface::onGetClipboardText;

void InputInterface::registerWindow(Window *window)
{
    s_windows.push_back(window);
}

void InputInterface::unregisterWindow(Window *window)
{
    if (s_lastClickWindow == window) {
        s_lastClickWindow = nullptr;
    }

    for (auto it = s_windows.begin(); it != s_windows.end(); ++it) {
        if (*it == window) {
            s_windows.erase(it);
            return;
        }
    }
}

void InputInterface::onMouseMove(Window *target, int32_t x, int32_t y)
{
    if (target != nullptr) {
        target->onMouseMove(x, y);
    }
}

void InputInterface::onMouseButton(Window *target, int button, int action, int mods, int32_t x, int32_t y)
{
    if (target == nullptr) {
        return;
    }

    if (action == MOUSE_ACTION_PRESS) {
        auto now = std::chrono::steady_clock::now();
        vec2 pos(static_cast<float>(x), static_cast<float>(y));
        float elapsedMs = std::chrono::duration<float, std::milli>(now - s_lastClickTime).count();
        if (target == s_lastClickWindow && button == s_lastClickButton && elapsedMs <= s_doubleClickIntervalMs &&
            length(pos - s_lastClickPos) <= DOUBLE_CLICK_DISTANCE) {
            mods |= MOD_DOUBLE_CLICK;
            s_lastClickButton = -1;
        } else {
            s_lastClickButton = button;
        }
        s_lastClickTime = now;
        s_lastClickPos = pos;
        s_lastClickWindow = target;
    }

    s_modifiers = mods;
    target->onMouseButton(button, action, mods, x, y);
}

void InputInterface::onMouseScroll(Window *target, float xoffset, float yoffset, int32_t x, int32_t y)
{
    if (target != nullptr) {
        target->onMouseScroll(xoffset, yoffset, x, y);
    }
}

int InputInterface::getModifiers()
{
    return s_modifiers;
}

void InputInterface::setCursorShape(CursorShape shape)
{
    if (s_currentCursorShape != shape) {
        s_currentCursorShape = shape;
        if (onCursorShapeChanged) {
            onCursorShapeChanged(s_currentCursorShape);
        }
    }
}

void InputInterface::setCursorLocked(bool locked)
{
    if (s_cursorLocked != locked) {
        s_cursorLocked = locked;
        if (onCursorLockChanged) {
            onCursorLockChanged(s_cursorLocked);
        }
    }
}

void InputInterface::onKey(int key, int scancode, int action, int mods)
{
    s_modifiers = mods;
    s_keyBuffer[s_keyBufferHead] = KeyEvent{key, scancode, static_cast<KeyAction>(action), mods};
    s_keyBufferHead = (s_keyBufferHead + 1) & (KEY_BUFFER_SIZE - 1);
}

void InputInterface::onChar(uint32_t codepoint)
{
    s_charBuffer[s_charBufferHead] = codepoint;
    s_charBufferHead = (s_charBufferHead + 1) & (CHAR_BUFFER_SIZE - 1);
}

bool InputInterface::pollKeyEvent(KeyEvent &outEvent)
{
    if (s_keyBufferTail == s_keyBufferHead) {
        return false;
    }

    outEvent = s_keyBuffer[s_keyBufferTail];
    s_keyBufferTail = (s_keyBufferTail + 1) & (KEY_BUFFER_SIZE - 1);
    return true;
}

bool InputInterface::pollCharEvent(uint32_t &outCodepoint)
{
    if (s_charBufferTail == s_charBufferHead) {
        return false;
    }

    outCodepoint = s_charBuffer[s_charBufferTail];
    s_charBufferTail = (s_charBufferTail + 1) & (CHAR_BUFFER_SIZE - 1);
    return true;
}

void InputInterface::clearKeyEvents()
{
    s_keyBufferHead = 0;
    s_keyBufferTail = 0;
    s_charBufferHead = 0;
    s_charBufferTail = 0;
}

void InputInterface::setClipboardText(const std::string &text)
{
    if (onSetClipboardText) {
        onSetClipboardText(text);
    }
}

std::string InputInterface::getClipboardText()
{
    if (onGetClipboardText) {
        return onGetClipboardText();
    }
    return "";
}

} // namespace Amethyst
