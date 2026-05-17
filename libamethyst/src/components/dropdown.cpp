#include "components/dropdown.h"

#include "components/scrolling_frame.h"
#include "rendering/draw_context.h"

namespace Amethyst {

Dropdown::Dropdown() = default;

void Dropdown::draw(DrawContext &)
{
    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void Dropdown::updateOptions()
{
}

EventResult Dropdown::onMouseButton1Click()
{
    m_popupOpen = !m_popupOpen;
    if (m_popup != nullptr) {
        m_popup->visible = m_popupOpen;
        m_popup->markDirty();
    }
    markDirty();
    return EventResult::CONSUMED;
}

} // namespace Amethyst
