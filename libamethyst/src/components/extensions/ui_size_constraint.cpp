#include "ui_size_constraint.h"

#include "components/ui_object.h"

namespace Amethyst {

void UISizeConstraint::apply()
{
    m_owner->absoluteSize = clamp(m_owner->absoluteSize, minSize, maxSize);
}

} // namespace Amethyst
