#include "components/checkbox.h"

#include "rendering/draw_context.h"

namespace Amethyst {

Checkbox::Checkbox()
{
    m_cbProps.checkColor = Color3{0.0f, 0.0f, 0.0f};
    m_cbProps.checkTransparency = 0.0f;
    m_cbProps.checkboxSize = 20.0f;
}

bool Checkbox::setCheckboxProperties(const CheckboxStyleProperties &props)
{
    bool changed = m_cbProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void Checkbox::draw(DrawContext &)
{
    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

EventResult Checkbox::onMouseButton1Click()
{
    if (valueRef != nullptr) {
        *valueRef = !(*valueRef);
        markDirty();
    }
    if (onValueChanged) {
        onValueChanged(valueRef != nullptr ? *valueRef : false);
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
