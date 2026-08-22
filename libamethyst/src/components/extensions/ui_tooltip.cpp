#include "components/extensions/ui_tooltip.h"

#include "components/tooltip.h"
#include "components/ui_object.h"
#include "components/window.h"

namespace Amethyst {

UITooltip::~UITooltip()
{
    if (m_scheduledOn != nullptr) {
        m_scheduledOn->getTooltips().cancel(m_owner);
    }
}

void UITooltip::handleMouseEnter()
{
    if (!enabled || !build) {
        return;
    }

    Window *window = m_owner->getWindow();
    if (window == nullptr) {
        return;
    }

    m_scheduledOn = window;
    window->getTooltips().schedule(m_owner, m_owner->absolutePosition, delaySeconds, build);
}

void UITooltip::handleMouseMove(int32_t x, int32_t y)
{
    if (m_scheduledOn == nullptr) {
        return;
    }
    m_scheduledOn->getTooltips().moveTo(vec2(static_cast<float>(x), static_cast<float>(y)));
}

void UITooltip::handleMouseLeave()
{
    if (m_scheduledOn == nullptr) {
        return;
    }
    m_scheduledOn->getTooltips().cancel(m_owner);
}

} // namespace Amethyst
