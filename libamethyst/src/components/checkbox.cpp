#include "components/checkbox.h"

#include "modules/style.h"
#include "rendering/draw_context.h"

namespace Amethyst {

Checkbox::Checkbox()
{
    resolveStyle();
}

void Checkbox::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::CHECKBOX, getClasses()));
    setCheckboxProperties(style.getCheckboxStyle(ComponentType::CHECKBOX, getClasses()));
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
