#include "components/ui_button.h"

namespace Amethyst {

bool UIButton::setButtonProperties(const ButtonProperties &props)
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
    UIObject::onInputBegan(input);

    uint32_t x = static_cast<uint32_t>(input.position.x);
    uint32_t y = static_cast<uint32_t>(input.position.y);
    switch (input.type) {
    case InputType::MOUSE_BUTTON_1:
        return onMouseButton1Down(x, y);
    case InputType::MOUSE_BUTTON_2:
        return onMouseButton2Down(x, y);
    default:
        return EventResult::CONSUMED;
    }
}

EventResult UIButton::onInputEnded(const InputObject &input)
{
    UIObject::onInputEnded(input);

    uint32_t x = static_cast<uint32_t>(input.position.x);
    uint32_t y = static_cast<uint32_t>(input.position.y);
    bool over = containsPoint(glm::vec2(input.position.x, input.position.y));
    switch (input.type) {
    case InputType::MOUSE_BUTTON_1: {
        EventResult up = onMouseButton1Up(x, y);
        if (!over) {
            return up;
        }
        EventResult click = onMouseButton1Click();
        return (up == EventResult::CONSUMED && click == EventResult::CONSUMED) ? EventResult::CONSUMED : EventResult::PROPAGATE;
    }
    case InputType::MOUSE_BUTTON_2: {
        EventResult up = onMouseButton2Up(x, y);
        if (!over) {
            return up;
        }
        EventResult click = onMouseButton2Click();
        return (up == EventResult::CONSUMED && click == EventResult::CONSUMED) ? EventResult::CONSUMED : EventResult::PROPAGATE;
    }
    default:
        return EventResult::CONSUMED;
    }
}

EventResult UIButton::onMouseButton1Down(uint32_t x, uint32_t y)
{
    if (onMouseButton1DownCb) {
        return onMouseButton1DownCb(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1Up(uint32_t x, uint32_t y)
{
    if (onMouseButton1UpCb) {
        return onMouseButton1UpCb(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1Click()
{
    if (onMouseButton1ClickCb) {
        return onMouseButton1ClickCb();
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Down(uint32_t x, uint32_t y)
{
    if (onMouseButton2DownCb) {
        return onMouseButton2DownCb(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Up(uint32_t x, uint32_t y)
{
    if (onMouseButton2UpCb) {
        return onMouseButton2UpCb(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton2Click()
{
    if (onMouseButton2ClickCb) {
        return onMouseButton2ClickCb();
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseEnter()
{
    UIObject::onMouseEnter();
    if (onMouseEnterCb) {
        return onMouseEnterCb();
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseLeave()
{
    UIObject::onMouseLeave();
    if (onMouseLeaveCb) {
        return onMouseLeaveCb();
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseMoved(uint32_t x, uint32_t y)
{
    UIObject::onMouseMoved(x, y);
    if (onMouseMovedCb) {
        return onMouseMovedCb(x, y);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
