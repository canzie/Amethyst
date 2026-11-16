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
    virtual EventResult onMouseButton1DoubleClick(int32_t x, int32_t y);
    virtual EventResult onMouseButton2Down(int32_t x, int32_t y);
    virtual EventResult onMouseButton2Up(int32_t x, int32_t y);
    virtual EventResult onMouseButton2Click();
    void onPressCancelled(InputType type) override;

    /**
     * @brief What a handled button event returns to dispatch, from the click input mode.
     */
    EventResult clickResult() const;

  protected:
    ButtonProperties m_btnProps;
    bool m_button2Pressed = false;

  public:
    // Listen-only, like UIObject's signals: any number of listeners, and whether the click carries on
    // past the button is the button's input mode (consume / propagate / ignore), not a listener's
    // return value. Hover and movement are UIObject's onHoverChanged and onInputChangedCb.
    EventSignal<void()> onMouseButton1ClickCb;
    EventSignal<void(int32_t, int32_t)> onMouseButton1DownCb;
    EventSignal<void(int32_t, int32_t)> onMouseButton1UpCb;
    EventSignal<void(int32_t, int32_t)> onMouseButton1DoubleClickCb;
    EventSignal<void()> onMouseButton2ClickCb;
    EventSignal<void(int32_t, int32_t)> onMouseButton2DownCb;
    EventSignal<void(int32_t, int32_t)> onMouseButton2UpCb;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BUTTON_H
