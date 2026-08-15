#include "ui_aspect_ratio_constraint.h"

namespace Amethyst {

vec2 UIAspectRatioConstraint::constrain(vec2 size) const
{
    if (dominantAxis == DominantAxis::WIDTH) {
        size.y = size.x / aspectRatio;
    } else {
        size.x = size.y * aspectRatio;
    }
    return size;
}

} // namespace Amethyst
