#include "components/ui_button.h"

namespace Amethyst {

EventResult UIButton::clickResult() const
{
    return consumes(INTERACTION_CATEGORY_CLICK) ? EventResult::CONSUMED : EventResult::PROPAGATE;
}

bool UIButton::setButtonProperties(const ButtonPropertiesArgs &props)
{
    bool changed = m_btnProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

const ButtonProperties &UIButton::getButtonProperties() const
{
    return m_btnProps;
}

EventResult UIButton::onInputBegan(const InputObject &input)
{
    std::weak_ptr<bool> alive = aliveToken();
    UIObject::onInputBegan(input);
    if (!isAlive(alive)) {
        return EventResult::CONSUMED;
    }

    int32_t x = static_cast<int32_t>(input.position.x);
    int32_t y = static_cast<int32_t>(input.position.y);
    switch (input.type) {
    case InputType::MOUSE_BUTTON_1:
        if ((input.modifiers & MOD_DOUBLE_CLICK) != 0) {
            onMouseButton1DoubleClick(x, y);
            if (!isAlive(alive)) {
                return EventResult::CONSUMED;
            }
        }
        return onMouseButton1Down(x, y);
    case InputType::MOUSE_BUTTON_2:
        return onMouseButton2Down(x, y);
    default:
        return EventResult::CONSUMED;
    }
}

EventResult UIButton::onInputEnded(const InputObject &input)
{
    std::weak_ptr<bool> alive = aliveToken();
    UIObject::onInputEnded(input);
    if (!isAlive(alive)) {
        return EventResult::CONSUMED;
    }

    int32_t x = static_cast<int32_t>(input.position.x);
    int32_t y = static_cast<int32_t>(input.position.y);
    bool over = containsPoint(vec2(input.position.x, input.position.y));
    switch (input.type) {
    case InputType::MOUSE_BUTTON_1: {
        // a release is only a click when this button took the press too, so a popup closing under the cursor
        // does not hand its release to whatever it was covering
        bool pressed = (getGuiState() & GUI_STATE_PRESSED) != 0;
        EventResult up = onMouseButton1Up(x, y);
        if (!isAlive(alive) || !over || !pressed) {
            return isAlive(alive) ? up : EventResult::CONSUMED;
        }
        EventResult click = onMouseButton1Click();
        return (up == EventResult::CONSUMED && click == EventResult::CONSUMED) ? EventResult::CONSUMED : EventResult::PROPAGATE;
    }
    case InputType::MOUSE_BUTTON_2: {
        // same rule as button 1: no click without this button having seen the press
        bool pressed = m_button2Pressed;
        m_button2Pressed = false;
        EventResult up = onMouseButton2Up(x, y);
        if (!isAlive(alive) || !over || !pressed) {
            return isAlive(alive) ? up : EventResult::CONSUMED;
        }
        EventResult click = onMouseButton2Click();
        return (up == EventResult::CONSUMED && click == EventResult::CONSUMED) ? EventResult::CONSUMED : EventResult::PROPAGATE;
    }
    default:
        return EventResult::CONSUMED;
    }
}

// Each internal step fires its signal for outside listeners; a listener may destroy the button, in
// which case fire() reports it and nothing here touches the button again.
EventResult UIButton::onMouseButton1Down(int32_t x, int32_t y)
{
    setGuiState(static_cast<uint16_t>(getGuiState() | GUI_STATE_PRESSED));
    if (!onMouseButton1DownCb.fire(x, y)) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton1Up(int32_t x, int32_t y)
{
    setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_PRESSED));
    if (!onMouseButton1UpCb.fire(x, y)) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton1Click()
{
    if (!onMouseButton1ClickCb.fire()) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton1DoubleClick(int32_t x, int32_t y)
{
    if (!onMouseButton1DoubleClickCb.fire(x, y)) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton2Down(int32_t x, int32_t y)
{
    m_button2Pressed = true;
    if (!onMouseButton2DownCb.fire(x, y)) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton2Up(int32_t x, int32_t y)
{
    if (!onMouseButton2UpCb.fire(x, y)) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

EventResult UIButton::onMouseButton2Click()
{
    if (!onMouseButton2ClickCb.fire()) {
        return EventResult::CONSUMED;
    }
    return clickResult();
}

void UIButton::onPressCancelled(InputType type)
{
    if (type == InputType::MOUSE_BUTTON_1) {
        setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_PRESSED));
    } else if (type == InputType::MOUSE_BUTTON_2) {
        m_button2Pressed = false;
    }
}

} // namespace Amethyst
