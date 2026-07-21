/*
 * Shape implementation
 */

#include "components/shape.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/ui_layer.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "logging/log.h"
#include "utils/profiling.h"

namespace Amethyst {

static PrimitiveType s_toPrimitiveType(ShapeKind kind)
{
    switch (kind) {
    case ShapeKind::CIRCLE:
        return PRIMITIVE_CIRCLE;
    case ShapeKind::RECT:
        return PRIMITIVE_RECT;
    case ShapeKind::TRIANGLE:
        return PRIMITIVE_TRIANGLE;
    default:
        AM_LOG_ERROR("Unhandled ShapeKind");
        return PRIMITIVE_RECT;
    }
}

Shape::Shape(ShapeKind kind) : m_kind(kind), m_primitive(s_toPrimitiveType(kind))
{
    markDirty();
}

bool Shape::setKind(ShapeKind kind)
{
    if (m_kind == kind) {
        return false;
    }
    m_kind = kind;
    m_primitive = s_toPrimitiveType(kind);
    markDirty();
    return true;
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

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
