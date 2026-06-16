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
    for (auto it = s_windows.begin(); it != s_windows.end(); ++it) {
        if (*it == window) {
            s_windows.erase(it);
            return;
        }
    }
}

void InputInterface::setMousePosition(int32_t x, int32_t y)
{
    s_mouseX = x;
    s_mouseY = y;

    for (Window *window : s_windows) {
        window->onMouseMove(s_mouseX, s_mouseY);
    }
}

void InputInterface::onMouseButton(int button, int action, int mods)
{
    s_modifiers = mods;
    for (Window *window : s_windows) {
        window->onMouseButton(button, action, mods, s_mouseX, s_mouseY);
    }
}

void InputInterface::onMouseScroll(float xoffset, float yoffset)
{
    for (Window *window : s_windows) {
        window->onMouseScroll(xoffset, yoffset, s_mouseX, s_mouseY);
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
