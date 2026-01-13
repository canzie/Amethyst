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

    void onMouseButton1Down(uint32_t x, uint32_t y) override
    {
        if (onMouseButton1DownCb) onMouseButton1DownCb(x, y);
    }
    void onMouseButton1Up(uint32_t x, uint32_t y) override
    {
        if (onMouseButton1UpCb) onMouseButton1UpCb(x, y);
    }
    void onMouseButton1Click() override
    {
        if (onMouseButton1ClickCb) onMouseButton1ClickCb();
    }
    void onMouseButton2Down(uint32_t x, uint32_t y) override
    {
        if (onMouseButton2DownCb) onMouseButton2DownCb(x, y);
    }
    void onMouseButton2Up(uint32_t x, uint32_t y) override
    {
        if (onMouseButton2UpCb) onMouseButton2UpCb(x, y);
    }
    void onMouseButton2Click() override
    {
        if (onMouseButton2ClickCb) onMouseButton2ClickCb();
    }

  public:
    std::function<void()> onMouseButton1ClickCb;
    std::function<void(uint32_t, uint32_t)> onMouseButton1DownCb;
    std::function<void(uint32_t, uint32_t)> onMouseButton1UpCb;
    std::function<void()> onMouseButton2ClickCb;
    std::function<void(uint32_t, uint32_t)> onMouseButton2DownCb;
    std::function<void(uint32_t, uint32_t)> onMouseButton2UpCb;

    bool autoButtonColor = true;
    bool modal = false;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
