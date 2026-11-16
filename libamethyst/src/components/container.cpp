#include "components/container.h"

#include "rendering/draw_context.h"

namespace Amethyst {

Container::Container()
{
    propagate(INTERACTION_CATEGORY_ALL);
}

void Container::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
