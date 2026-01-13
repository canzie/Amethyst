/*
 * Input interface implementation
 */

#include "components/input_interface.h"
#include "components/window.h"

namespace Amethyst {

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

void InputInterface::setMousePosition(uint32_t x, uint32_t y)
{
    s_mouseX = x;
    s_mouseY = y;

    for (Window *window : s_windows) {
        window->onMouseMove(s_mouseX, s_mouseY);
    }
}

void InputInterface::onMouseButton(int button, int action, int mods)
{
    (void)mods;
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

} // namespace Amethyst
