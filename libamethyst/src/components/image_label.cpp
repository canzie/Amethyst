/*
 * ImageLabel implementation
 */

#include "components/image_label.h"

#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

void ImageLabel::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;
        data.textureId = image.id;

        if (m_allocationIndex == UINT32_MAX) {
            m_allocationIndex = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(m_allocationIndex, data);
        }
    }

    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
