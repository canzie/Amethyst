/*
 * Base class for button UI elements
 */

#ifndef AMETHYST__UI_BUTTON_H
#define AMETHYST__UI_BUTTON_H

#include "components/properties.h"
#include "components/ui_object.h"
#include <cstdint>
#include <functional>

namespace Amethyst {

class UIButton : public UIObject {
  public:
    UIButton() = default;
    virtual ~UIButton() = default;

    bool setButtonProperties(const ButtonProperties &props);
    const ButtonProperties &getButtonProperties() const;

  protected:
    EventResult onMouseButton1Down(uint32_t x, uint32_t y) override;
    EventResult onMouseButton1Up(uint32_t x, uint32_t y) override;
    EventResult onMouseButton1Click() override;
    EventResult onMouseButton2Down(uint32_t x, uint32_t y) override;
    EventResult onMouseButton2Up(uint32_t x, uint32_t y) override;
    EventResult onMouseButton2Click() override;
    EventResult onMouseEnter() override;
    EventResult onMouseLeave() override;
    EventResult onMouseMoved(uint32_t x, uint32_t y) override;

  protected:
    ButtonProperties m_btnProps;

  public:
    std::function<EventResult()> onMouseButton1ClickCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton1DownCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton1UpCb;
    std::function<EventResult()> onMouseButton2ClickCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton2DownCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseButton2UpCb;
    std::function<EventResult()> onMouseEnterCb;
    std::function<EventResult()> onMouseLeaveCb;
    std::function<EventResult(uint32_t, uint32_t)> onMouseMovedCb;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
