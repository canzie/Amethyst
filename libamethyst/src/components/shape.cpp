/*
 * Shape implementation
 */

#include "components/shape.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/ui_layer.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

Shape::Shape(PrimitiveType primitive) : m_primitive(primitive)
{
    markDirty();
}

void Shape::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(m_primitive);

        pushData(ctx.geometry, data);
    }

    if (auto *gridLayout = getExtension<UIGridLayout>()) {
        gridLayout->apply(m_children);
    } else if (auto *listLayout = getExtension<UIListLayout>()) {
        listLayout->apply(m_children);
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        } else if (auto *layer = child->as<UILayer>()) {
            layer->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
