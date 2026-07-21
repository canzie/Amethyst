/*
 * UILayer implementation
 */

#include "components/ui_layer.h"

#include "components/ui_object.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

UILayer::UILayer()
{
    kind |= KIND_UI_LAYER;
    m_geometryRegistry = GeometryRegistry::create(this);
}

UILayer::~UILayer()
{
    m_children.clear();
}

void UILayer::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->clipRect = clipRect;
            obj->computeAbsolutes(absoluteSize, absolutePosition, absoluteRotation);
            obj->arrange();
        } else if (auto *layer = child->asLayer()) {
            layer->arrange();
        }
    }
}

bool UILayer::isVisible() const
{
    if (!visible) return false;
    if (parent == nullptr) return true;
    if (auto *obj = parent->asUiObject()) {
        return obj->isVisible();
    } else if (auto *layer = parent->asLayer()) {
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
