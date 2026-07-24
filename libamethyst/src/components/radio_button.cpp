#include "components/radio_button.h"

#include "modules/style.h"
#include "rendering/draw_context.h"

namespace Amethyst {

RadioButton::RadioButton()
{
    resolveStyle();
}

void RadioButton::setGroup(RadioGroup *group)
{
    if (m_group == group) {
        return;
    }
    m_group = group;
    m_groupConn.disconnect();
    if (m_group != nullptr) {
        m_groupConn = m_group->onChanged.connect([this]() { markDirty(); });
    }
    markDirty();
}

void RadioButton::arrange()
{
    bool selected = m_group != nullptr && m_group->value == value;
    uint16_t state = getGuiState();
    setGuiState(static_cast<uint16_t>(selected ? (state | GUI_STATE_ACTIVE) : (state & ~GUI_STATE_ACTIVE)));
    UIObject::arrange();
}

void RadioButton::resolveStyle()
{
    resolveBaseStyle(ComponentType::RADIO_BUTTON);
}

void RadioButton::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_CIRCLE);

        pushData(ctx.geometry, data);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

EventResult RadioButton::onMouseButton1Click()
{
    if (m_group != nullptr) {
        m_group->select(value);
    }
    onSelected.fire(value);
    return EventResult::CONSUMED;
}

} // namespace Amethyst
