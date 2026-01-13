/*
 * Frame implementation
 */

#include "components/frame.h"

#include "components/ui_object.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

void Frame::draw(GeometryRegistry &registry)
{
    if (!(flags & FLAG_DIRTY)) return;

    InstanceData data = createInstanceData();
    data.primitiveType = PRIMITIVE_RECT;

    if (m_allocationIndex == UINT32_MAX) {
        m_allocationIndex = registry.submit(data);
    } else {
        registry.update(m_allocationIndex, data);
    }
    for (Instance *child : children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            drawable->draw(registry);
        }
    }

    flags &= ~FLAG_DIRTY;
}

} // namespace Amethyst
