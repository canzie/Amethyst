#include "ui_aspect_ratio_constraint.h"

#include "components/ui_object.h"

namespace Amethyst {

void UIAspectRatioConstraint::apply()
{
    if (dominantAxis == DominantAxis::WIDTH) {
        m_owner->absoluteSize.y = m_owner->absoluteSize.x / aspectRatio;
    } else {
        m_owner->absoluteSize.x = m_owner->absoluteSize.y * aspectRatio;
    }
}

} // namespace Amethyst
