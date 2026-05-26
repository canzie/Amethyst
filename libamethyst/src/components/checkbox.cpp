#include "components/checkbox.h"

#include "rendering/draw_context.h"

namespace Amethyst {

Checkbox::Checkbox()
{
    m_cbProps.labelSide = LabelSide::RIGHT;
    m_cbProps.labelColor = Color4{0.0f, 0.0f, 0.0f, 1.0f};
    m_cbProps.labelXAlignment = TextXAlignment::LEFT;
    m_cbProps.labelYAlignment = TextYAlignment::CENTER;
    m_cbProps.checkColor = Color3{0.0f, 0.0f, 0.0f};
    m_cbProps.checkTransparency = 0.0f;
    m_cbProps.checkboxSize = 20.0f;
    m_cbProps.labelPadding = UDim::fromOffset(5.0f);
}

bool Checkbox::setCheckboxProperties(const CheckboxProperties &props)
{
    bool changed = false;
#define AM_APPLY(field) \
    if (propIsSet(props.field) && m_cbProps.field != props.field) { \
        m_cbProps.field = props.field; \
        changed = true; \
    }
    AM_APPLY(labelSide)
    AM_APPLY(labelColor)
    AM_APPLY(labelXAlignment)
    AM_APPLY(labelYAlignment)
    AM_APPLY(checkColor)
    AM_APPLY(checkTransparency)
    AM_APPLY(checkboxSize)
    AM_APPLY(labelPadding)
#undef AM_APPLY
    if (!props.label.empty() && m_cbProps.label != props.label) {
        m_cbProps.label = props.label;
        changed = true;
    }
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
