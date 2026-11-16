#include "components/ui_button.h"

namespace Amethyst {

// Callbacks are invoked through a copy: a handler that destroys its own button would otherwise
// free the std::function (and the closure's captures) while it is still running.
template <typename Fn, typename... Args> static EventResult s_invoke(const Fn &fn, Args... args)
{
    Fn copy = fn;
    return copy(args...);
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

EventResult UIButton::onMouseButton1Down(int32_t x, int32_t y)
{
    setGuiState(static_cast<uint16_t>(getGuiState() | GUI_STATE_PRESSED));
    if (onMouseButton1DownCb) {
        return s_invoke(onMouseButton1DownCb, x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1Up(int32_t x, int32_t y)
{
    setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_PRESSED));
    if (onMouseButton1UpCb) {
        return s_invoke(onMouseButton1UpCb, x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1Click()
{
    if (onMouseButton1ClickCb) {
        return s_invoke(onMouseButton1ClickCb);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1DoubleClick(int32_t x, int32_t y)
{
    if (onMouseButton1DoubleClickCb) {
        return s_invoke(onMouseButton1DoubleClickCb, x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Down(int32_t x, int32_t y)
{
    m_button2Pressed = true;
    if (onMouseButton2DownCb) {
        return s_invoke(onMouseButton2DownCb, x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Up(int32_t x, int32_t y)
{
    if (onMouseButton2UpCb) {
        return s_invoke(onMouseButton2UpCb, x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Click()
{
    if (onMouseButton2ClickCb) {
        return s_invoke(onMouseButton2ClickCb);
    }
    return EventResult::CONSUMED;
}

void UIButton::onPressCancelled(InputType type)
{
    if (type == InputType::MOUSE_BUTTON_1) {
        setGuiState(static_cast<uint16_t>(getGuiState() & ~GUI_STATE_PRESSED));
    } else if (type == InputType::MOUSE_BUTTON_2) {
        m_button2Pressed = false;
    }
}

EventResult UIButton::onMouseEnter()
{
    std::weak_ptr<bool> alive = aliveToken();
    UIObject::onMouseEnter();
    if (!isAlive(alive)) {
        return EventResult::CONSUMED;
    }
    if (onMouseEnterCb) {
        return s_invoke(onMouseEnterCb);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseLeave()
{
    std::weak_ptr<bool> alive = aliveToken();
    UIObject::onMouseLeave();
    if (!isAlive(alive)) {
        return EventResult::CONSUMED;
    }
    if (onMouseLeaveCb) {
        return s_invoke(onMouseLeaveCb);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseMoved(int32_t x, int32_t y)
{
    std::weak_ptr<bool> alive = aliveToken();
    UIObject::onMouseMoved(x, y);
    if (!isAlive(alive)) {
        return EventResult::CONSUMED;
    }
    if (onMouseMovedCb) {
        return s_invoke(onMouseMovedCb, x, y);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
