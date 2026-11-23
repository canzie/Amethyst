#include "components/ui_base_2d.h"

#include "rendering/geometry_registry.h"

namespace Amethyst {

void UIBase2D::inheritTransform()
{
    // Window, layers and UI objects are the only UIBase2D kinds, and both flags sit on the same base
    const Instance *above = parent;
    if (above != nullptr && (above->kind & (KIND_UI_OBJECT | KIND_UI_LAYER)) != 0) {
        const auto *base = reinterpret_cast<const UIBase2D *>(above);
        absoluteScale = base->absoluteScale;
        screenOffset = base->screenOffset;
    } else {
        absoluteScale = 1.0f;
        screenOffset = vec2(0.0f);
    }
}

void UIBase2D::mapToScreen(InstanceData &data) const
{
    data.transform(absoluteScale, screenOffset, true);
}

GeometryAllocation *UIBase2D::pushData(GeometryRegistry *registry, const InstanceData &layoutData)
{
    InstanceData data = layoutData;
    mapToScreen(data);
    if (m_geometryAlloc == nullptr) {
        m_geometryAlloc = registry->submit(data);
    } else if (m_geometryAlloc->registry != registry) {
        if (m_geometryAlloc->isValid() && m_geometryAlloc->owning) {
            m_geometryAlloc->registry->release(*m_geometryAlloc);
        }
        m_geometryAlloc = registry->submit(data);
    } else {
        registry->update(*m_geometryAlloc, data);
    }
    return m_geometryAlloc;
}

} // namespace Amethyst
