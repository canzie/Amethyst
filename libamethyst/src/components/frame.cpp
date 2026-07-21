/*
 * Frame implementation
 */

#include "components/frame.h"

#include "modules/style.h"
#include "rendering/draw_context.h"
#include "utils/profiling.h"

namespace Amethyst {

Frame::Frame()
{
    resolveStyle();
}

void Frame::resolveStyle()
{
    setBaseStyleProperties(Style::instance().getBaseStyle(ComponentType::FRAME, getClasses()));
}

void Frame::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        pushData(ctx.geometry, data);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
