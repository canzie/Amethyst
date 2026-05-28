#include "components/ui_button.h"

namespace Amethyst {

bool UIButton::setButtonProperties(const ButtonProperties &props)
{
    bool changed = false;
#define AM_APPLY(field)                                              \
    if (propIsSet(props.field) && m_btnProps.field != props.field) { \
        m_btnProps.field = props.field;                              \
        changed = true;                                              \
    }
    AM_APPLY(autoButtonColor)
    AM_APPLY(modal)
#undef AM_APPLY
    if (changed) {
        markDirty();
    }
    return changed;
}

const ButtonProperties &UIButton::getButtonProperties() const
{
    return m_btnProps;
}

EventResult UIButton::onMouseButton1Down(uint32_t x, uint32_t y)
{
    UIObject::onMouseButton1Down(x, y);
    if (onMouseButton1DownCb) {
        return onMouseButton1DownCb(x, y);
    }
    return EventResult::CONSUMED;
}

EventResult UIButton::onMouseButton1Up(uint32_t x, uint32_t y)
{
    UIObject::onMouseButton1Up(x, y);
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
