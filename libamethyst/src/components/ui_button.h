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

    bool setButtonProperties(const ButtonPropertiesArgs &props);
    const ButtonProperties &getButtonProperties() const;

  protected:
    EventResult onInputBegan(const InputObject &input) override;
    EventResult onInputEnded(const InputObject &input) override;

    virtual EventResult onMouseButton1Down(int32_t x, int32_t y);
    virtual EventResult onMouseButton1Up(int32_t x, int32_t y);
    virtual EventResult onMouseButton1Click();
    virtual EventResult onMouseButton2Down(int32_t x, int32_t y);
    virtual EventResult onMouseButton2Up(int32_t x, int32_t y);
    virtual EventResult onMouseButton2Click();
    EventResult onMouseEnter() override;
    EventResult onMouseLeave() override;
    EventResult onMouseMoved(int32_t x, int32_t y) override;

  protected:
    ButtonProperties m_btnProps;

  public:
    std::function<EventResult()> onMouseButton1ClickCb;
    std::function<EventResult(int32_t, int32_t)> onMouseButton1DownCb;
    std::function<EventResult(int32_t, int32_t)> onMouseButton1UpCb;
    std::function<EventResult()> onMouseButton2ClickCb;
    std::function<EventResult(int32_t, int32_t)> onMouseButton2DownCb;
    std::function<EventResult(int32_t, int32_t)> onMouseButton2UpCb;
    std::function<EventResult()> onMouseEnterCb;
    std::function<EventResult()> onMouseLeaveCb;
    std::function<EventResult(int32_t, int32_t)> onMouseMovedCb;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
