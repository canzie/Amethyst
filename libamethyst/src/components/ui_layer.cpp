/*
 * UILayer implementation
 */

#include "components/ui_layer.h"

#include "components/ui_object.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

UILayer::UILayer()
{
    m_geometryRegistry = GeometryRegistry::create(this);
}

UILayer::~UILayer()
{
    m_children.clear();
}

bool UILayer::isVisible() const
{
    if (!visible) return false;
    if (parent == nullptr) return true;
    if (auto *obj = parent->as<UIObject>()) {
        return obj->isVisible();
    } else if (auto *layer = parent->as<UILayer>()) {
        return layer->isVisible();
    }
    return true;
}

void UILayer::setDisplayOrder(int32_t order)
{
    if (m_displayOrder == order) {
        return;
    }
    m_displayOrder = order;
    GeometryRegistry::resortRegistries();
}

} // namespace Amethyst
