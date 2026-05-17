#include "components/checkbox.h"

#include "rendering/draw_context.h"

namespace Amethyst {

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
