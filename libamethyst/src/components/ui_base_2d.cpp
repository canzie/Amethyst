#include "components/ui_base_2d.h"

#include "rendering/geometry_registry.h"

namespace Amethyst {

GeometryAllocation *UIBase2D::pushData(GeometryRegistry *registry, const InstanceData &data)
{
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
