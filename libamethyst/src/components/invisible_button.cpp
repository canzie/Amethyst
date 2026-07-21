/*
 * InvisibleButton implementation
 */

#include "components/invisible_button.h"

namespace Amethyst {

InvisibleButton::~InvisibleButton() = default;

void InvisibleButton::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
