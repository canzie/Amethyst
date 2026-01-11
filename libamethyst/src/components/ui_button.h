/*
 * Base class for button UI elements
 */

#ifndef AMETHYST__UI_BUTTON_H
#define AMETHYST__UI_BUTTON_H

#include "components/ui_object.h"
#include <cstdint>
#include <functional>

namespace Amethyst {

class UIButton : public UIObject {
  public:
    UIButton() = default;
    virtual ~UIButton() = default;

    void MouseButton1Click() { if (onMouseButton1Click) onMouseButton1Click(); }
    void MouseButton1Down(uint32_t x, uint32_t y) { if (onMouseButton1Down) onMouseButton1Down(x, y); }
    void MouseButton1Up(uint32_t x, uint32_t y) { if (onMouseButton1Up) onMouseButton1Up(x, y); }
    void MouseButton2Click() { if (onMouseButton2Click) onMouseButton2Click(); }
    void MouseButton2Down(uint32_t x, uint32_t y) { if (onMouseButton2Down) onMouseButton2Down(x, y); }
    void MouseButton2Up(uint32_t x, uint32_t y) { if (onMouseButton2Up) onMouseButton2Up(x, y); }

  public:
    std::function<void()> onMouseButton1Click;
    std::function<void(uint32_t, uint32_t)> onMouseButton1Down;
    std::function<void(uint32_t, uint32_t)> onMouseButton1Up;
    std::function<void()> onMouseButton2Click;
    std::function<void(uint32_t, uint32_t)> onMouseButton2Down;
    std::function<void(uint32_t, uint32_t)> onMouseButton2Up;

    bool autoButtonColor = true;
    bool modal = false;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
