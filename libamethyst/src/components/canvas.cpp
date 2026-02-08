#include "components/canvas.h"

#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"

#include <algorithm>
#include <glm/glm.hpp>

namespace Amethyst {

Canvas::Canvas(Instance *parent)
{
    setParent(parent);
}

Canvas::~Canvas()
{
    if (m_registry) {
        for (auto *alloc : m_allocations) {
            m_registry->release(*alloc);
        }
    }
}

void Canvas::clear()
{
    if (!m_commands.empty()) {
        m_commands.clear();
        markDirty();
    }
}

void Canvas::drawLine(glm::vec2 start, glm::vec2 end, Color4 color, float thickness)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_LINE;
    cmd.points[0] = start;
    cmd.points[1] = end;
    cmd.fillColor = color;
    cmd.borderThickness = thickness;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawTriangleFilled(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_TRI;
    cmd.points[0] = p0;
    cmd.points[1] = p1;
    cmd.points[2] = p2;
    cmd.fillColor = color;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawTriangleStroke(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color, float thickness)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_TRI;
    cmd.points[0] = p0;
    cmd.points[1] = p1;
    cmd.points[2] = p2;
    cmd.fillColor = Color4(0.0f, 0.0f, 0.0f, 0.0f);
    cmd.borderColor = color;
    cmd.borderThickness = thickness;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawQuadFilled(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color4 color)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_QUAD;
    cmd.points[0] = p0;
    cmd.points[1] = p1;
    cmd.points[2] = p2;
    cmd.points[3] = p3;
    cmd.fillColor = color;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawQuadStroke(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color4 color, float thickness)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_QUAD;
    cmd.points[0] = p0;
    cmd.points[1] = p1;
    cmd.points[2] = p2;
    cmd.points[3] = p3;
    cmd.fillColor = Color4(0.0f, 0.0f, 0.0f, 0.0f);
    cmd.borderColor = color;
    cmd.borderThickness = thickness;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawCircleFilled(glm::vec2 center, float radius, Color4 color)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_CIRCLE;
    cmd.points[0] = center;
    cmd.radius = radius;
    cmd.fillColor = color;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawCircleStroke(glm::vec2 center, float radius, Color4 color, float thickness)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_CIRCLE;
    cmd.points[0] = center;
    cmd.radius = radius;
    cmd.fillColor = Color4(0.0f, 0.0f, 0.0f, 0.0f);
    cmd.borderColor = color;
    cmd.borderThickness = thickness;
    m_commands.push_back(cmd);
    markDirty();
}

InstanceData Canvas::buildInstanceData(const DrawCmd &cmd) const
{
    InstanceData data{};
    data.clipRect = clipRect;
    data.setPrimitiveType(cmd.primitive);
    data.setFillColor(cmd.fillColor);
    data.setBorderColor(cmd.borderColor);
    data.zIndex = getZIndex();
    data.setVisible(isVisible());
    data.setBorderMode(BorderMode::MIDDLE);

    float padding = std::max(cmd.borderThickness, 1.0f);

    if (cmd.primitive == PRIMITIVE_CANVAS_LINE) {
        glm::vec2 bboxMin = glm::min(cmd.points[0], cmd.points[1]);
        glm::vec2 bboxMax = glm::max(cmd.points[0], cmd.points[1]);

        float halfThick = cmd.borderThickness * 0.5f;
        bboxMin -= glm::vec2(halfThick + padding);
        bboxMax += glm::vec2(halfThick + padding);

        glm::vec2 center = (bboxMin + bboxMax) * 0.5f;
        glm::vec2 size = bboxMax - bboxMin;

        data.translation = absolutePosition + center;
        data.scale = size;
        data.setCornerRadius(halfThick);
        data.setShapePoint(0, cmd.points[0] - center);
        data.setShapePoint(1, cmd.points[1] - center);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_TRI) {
        glm::vec2 bboxMin = glm::min(glm::min(cmd.points[0], cmd.points[1]), cmd.points[2]);
        glm::vec2 bboxMax = glm::max(glm::max(cmd.points[0], cmd.points[1]), cmd.points[2]);

        bboxMin -= glm::vec2(padding);
        bboxMax += glm::vec2(padding);

        glm::vec2 center = (bboxMin + bboxMax) * 0.5f;
        glm::vec2 size = bboxMax - bboxMin;

        data.translation = absolutePosition + center;
        data.scale = size;
        data.setBorderThickness(cmd.borderThickness);
        data.setShapePoint(0, cmd.points[0] - center);
        data.setShapePoint(1, cmd.points[1] - center);
        data.setShapePoint(2, cmd.points[2] - center);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_QUAD) {
        glm::vec2 bboxMin = glm::min(glm::min(cmd.points[0], cmd.points[1]), glm::min(cmd.points[2], cmd.points[3]));
        glm::vec2 bboxMax = glm::max(glm::max(cmd.points[0], cmd.points[1]), glm::max(cmd.points[2], cmd.points[3]));

        bboxMin -= glm::vec2(padding);
        bboxMax += glm::vec2(padding);

        glm::vec2 center = (bboxMin + bboxMax) * 0.5f;
        glm::vec2 size = bboxMax - bboxMin;

        data.translation = absolutePosition + center;
        data.scale = size;
        data.setBorderThickness(cmd.borderThickness);
        data.setShapePoint(0, cmd.points[0] - center);
        data.setShapePoint(1, cmd.points[1] - center);
        data.setShapePoint(2, cmd.points[2] - center);
        data.setShapePoint(3, cmd.points[3] - center);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_CIRCLE) {
        float totalRadius = cmd.radius + padding;

        data.translation = absolutePosition + cmd.points[0];
        data.scale = glm::vec2(totalRadius * 2.0f);
        data.setCornerRadius(cmd.radius);
        data.setBorderThickness(cmd.borderThickness);
    }

    return data;
}

void Canvas::draw(DrawContext &ctx)
{
    if (!(flags & FLAG_DIRTY)) {
        return;
    }

    m_registry = ctx.geometry;

    size_t newCount = m_commands.size();

    while (m_allocations.size() > newCount) {
        ctx.geometry->release(*m_allocations.back());
        m_allocations.pop_back();
    }

    for (size_t i = 0; i < m_allocations.size(); i++) {
        InstanceData data = buildInstanceData(m_commands[i]);
        ctx.geometry->update(*m_allocations[i], data);
    }

    for (size_t i = m_allocations.size(); i < newCount; i++) {
        InstanceData data = buildInstanceData(m_commands[i]);
        m_allocations.push_back(ctx.geometry->submit(data));
    }

    flags &= ~FLAG_DIRTY;
}

} // namespace Amethyst
