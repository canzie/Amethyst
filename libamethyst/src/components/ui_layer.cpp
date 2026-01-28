/*
 * UILayer implementation
 */

#include "components/ui_layer.h"

#include "rendering/geometry_registry.h"

namespace Amethyst {

UILayer::UILayer()
{
    m_geometryRegistry = GeometryRegistry::create(this);
}

UILayer::~UILayer() = default;

void UILayer::setDisplayOrder(int32_t order)
{
    if (m_displayOrder == order) {
        return;
    }
    m_displayOrder = order;
    GeometryRegistry::resortRegistries();
}

} // namespace Amethyst
