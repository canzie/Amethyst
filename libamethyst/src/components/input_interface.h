/*
 * Input interface for backend integration
 */

#ifndef AMETHYST__INPUT_INTERFACE_H
#define AMETHYST__INPUT_INTERFACE_H

#include <cstdint>
#include <vector>

namespace Amethyst {

class Window;

class InputInterface {
  public:
    static void registerWindow(Window *window);
    static void unregisterWindow(Window *window);

    static void setMousePosition(uint32_t x, uint32_t y);
    static void onMouseButton(int button, int action, int mods);
    static void onMouseScroll(float xoffset, float yoffset);

  private:
    static inline std::vector<Window *> s_windows;
    static inline uint32_t s_mouseX = 0;
    static inline uint32_t s_mouseY = 0;
    static inline bool isWindowFocussed = false;
};

} // namespace Amethyst

#endif // AMETHYST__INPUT_INTERFACE_H
