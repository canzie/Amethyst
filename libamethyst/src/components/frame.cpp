/*
 * Frame implementation
 */

#include "components/frame.h"

#include "components/ui_object.h"
#include "logging/log.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

void Frame::onMouseButton1Click()
{
    AM_LOG_INFO("Frame '{}' clicked (button 1)", name.empty() ? "(unnamed)" : name);
}

void Frame::onMouseButton2Click()
{
    AM_LOG_INFO("Frame '{}' clicked (button 2)", name.empty() ? "(unnamed)" : name);
}

void Frame::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.primitiveType = PRIMITIVE_RECT;

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
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
