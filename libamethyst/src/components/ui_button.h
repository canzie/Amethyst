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

  protected:
    EventResult onMouseButton1Down(uint32_t x, uint32_t y) override
    {
        UIObject::onMouseButton1Down(x, y);
        if (onMouseButton1DownCb) return onMouseButton1DownCb(x, y);
        return EventResult::CONSUMED;
    }

    EventResult onMouseButton1Up(uint32_t x, uint32_t y) override
    {
        UIObject::onMouseButton1Up(x, y);
        if (onMouseButton1UpCb) return onMouseButton1UpCb(x, y);
        return EventResult::CONSUMED;
    }

    EventResult onMouseButton1Click(void) override
    {
        if (onMouseButton1ClickCb) return onMouseButton1ClickCb();
        return EventResult::CONSUMED;
    }

    EventResult onMouseButton2Down(uint32_t x, uint32_t y) override
    {
        if (onMouseButton2DownCb) return onMouseButton2DownCb(x, y);
        return EventResult::CONSUMED;
    }

    EventResult onMouseButton2Up(uint32_t x, uint32_t y) override
    {
        if (onMouseButton2UpCb) return onMouseButton2UpCb(x, y);
        return EventResult::CONSUMED;
    }

    EventResult onMouseButton2Click(void) override
    {
        if (onMouseButton2ClickCb) return onMouseButton2ClickCb();
        return EventResult::CONSUMED;
    }

    EventResult onMouseEnter(void) override
    {
        if (onMouseEnterCb) return onMouseEnterCb();
        return EventResult::CONSUMED;
    }

    EventResult onMouseLeave(void) override
    {
        if (onMouseLeaveCb) return onMouseLeaveCb();
        return EventResult::CONSUMED;
    }

  public:
    std::function<EventResult()> onMouseButton1ClickCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton1DownCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton1UpCb;
    std::function<EventResult()> onMouseButton2ClickCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton2DownCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton2UpCb;
    std::function<EventResult()> onMouseEnterCb;
    std::function<EventResult()> onMouseLeaveCb;

    bool autoButtonColor = true;
    bool modal = false;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
