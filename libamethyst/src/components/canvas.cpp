#include "components/canvas.h"

#include "modules/glyph_atlas.h"
#include "modules/text_processor.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"

#include "math/math.h"
#include <algorithm>

namespace Amethyst {

Canvas::~Canvas()
{
    if (m_registry) {
        for (auto *alloc : m_allocations) {
            m_registry->release(*alloc);
        }
        for (auto *alloc : m_textAllocations) {
            m_registry->release(*alloc);
        }
    }
}

void Canvas::clear()
{
    if (!m_commands.empty() || !m_textCommands.empty()) {
        m_commands.clear();
        m_textCommands.clear();
        markDirty();
    }
}

void Canvas::drawLine(vec2 start, vec2 end, Color4 color, float thickness)
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

void Canvas::drawTriangleFilled(vec2 p0, vec2 p1, vec2 p2, Color4 color)
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

void Canvas::drawTriangleStroke(vec2 p0, vec2 p1, vec2 p2, Color4 color, float thickness)
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

void Canvas::drawQuadFilled(vec2 p0, vec2 p1, vec2 p2, vec2 p3, Color4 color)
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

void Canvas::drawQuadStroke(vec2 p0, vec2 p1, vec2 p2, vec2 p3, Color4 color, float thickness)
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

void Canvas::drawCircleFilled(vec2 center, float radius, Color4 color)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_CIRCLE;
    cmd.points[0] = center;
    cmd.radius = radius;
    cmd.fillColor = color;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawCircleStroke(vec2 center, float radius, Color4 color, float thickness)
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

void Canvas::drawEllipseStroke(vec2 center, float semiMajor, float semiMinor, float rotationDeg, Color4 color, float thickness)
{
    DrawCmd cmd{};
    cmd.primitive = PRIMITIVE_CANVAS_ELLIPSE;
    cmd.points[0] = center;
    cmd.semiMajor = semiMajor;
    cmd.semiMinor = semiMinor;
    cmd.rotationDeg = rotationDeg;
    cmd.fillColor = Color4(0.0f, 0.0f, 0.0f, 0.0f);
    cmd.borderColor = color;
    cmd.borderThickness = thickness;
    m_commands.push_back(cmd);
    markDirty();
}

void Canvas::drawText(const std::string &text, vec2 position, float fontSize, Color4 color, uint32_t maxChars)
{
    TextCmd cmd;
    cmd.text = text;
    cmd.position = position;
    cmd.fontSize = fontSize;
    cmd.color = color;
    cmd.maxChars = maxChars;
    m_textCommands.push_back(std::move(cmd));
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
        vec2 bboxMin = min(cmd.points[0], cmd.points[1]);
        vec2 bboxMax = max(cmd.points[0], cmd.points[1]);

        float halfThick = cmd.borderThickness * 0.5f;
        bboxMin -= vec2(halfThick + padding);
        bboxMax += vec2(halfThick + padding);

        vec2 center = (bboxMin + bboxMax) * 0.5f;
        vec2 size = bboxMax - bboxMin;

        data.translation = absolutePosition + center;
        data.scale = size;
        data.setThickness(halfThick);
        data.setShapePoint(0, cmd.points[0] - center);
        data.setShapePoint(1, cmd.points[1] - center);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_TRI) {
        vec2 bboxMin = min(min(cmd.points[0], cmd.points[1]), cmd.points[2]);
        vec2 bboxMax = max(max(cmd.points[0], cmd.points[1]), cmd.points[2]);

        bboxMin -= vec2(padding);
        bboxMax += vec2(padding);

        vec2 center = (bboxMin + bboxMax) * 0.5f;
        vec2 size = bboxMax - bboxMin;

        data.translation = absolutePosition + center;
        data.scale = size;
        data.setBorderThickness(cmd.borderThickness);
        data.setShapePoint(0, cmd.points[0] - center);
        data.setShapePoint(1, cmd.points[1] - center);
        data.setShapePoint(2, cmd.points[2] - center);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_QUAD) {
        vec2 bboxMin = min(min(cmd.points[0], cmd.points[1]), min(cmd.points[2], cmd.points[3]));
        vec2 bboxMax = max(max(cmd.points[0], cmd.points[1]), max(cmd.points[2], cmd.points[3]));

        bboxMin -= vec2(padding);
        bboxMax += vec2(padding);

        vec2 center = (bboxMin + bboxMax) * 0.5f;
        vec2 size = bboxMax - bboxMin;

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
        data.scale = vec2(totalRadius * 2.0f);
        data.setThickness(cmd.radius);
        data.setBorderThickness(cmd.borderThickness);

    } else if (cmd.primitive == PRIMITIVE_CANVAS_ELLIPSE) {
        float totalW = cmd.semiMajor + padding + cmd.borderThickness;
        float totalH = cmd.semiMinor + padding + cmd.borderThickness;

        data.translation = absolutePosition + cmd.points[0];
        data.scale = vec2(totalW * 2.0f, totalH * 2.0f);
        data.setRotation(cmd.rotationDeg * 3.14159265359f / 180.0f);
        data.setBorderThickness(cmd.borderThickness);
        data.setShapePoint(0, vec2(cmd.semiMajor, cmd.semiMinor));
    }

    return data;
}

void Canvas::draw(DrawContext &ctx)
{
    if (!(flags & FLAG_DIRTY)) {
        return;
    }

    if (m_registry != nullptr && m_registry != ctx.geometry) {
        for (GeometryAllocation *a : m_allocations) {
            if (a->isValid()) {
                a->registry->release(*a);
            }
        }
        m_allocations.clear();
        for (GeometryAllocation *a : m_textAllocations) {
            if (a->isValid()) {
                a->registry->release(*a);
            }
        }
        m_textAllocations.clear();
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

    if (ctx.textProcessor && ctx.glyphAtlas) {
        size_t totalReserved = 0;
        for (const auto &tcmd : m_textCommands) {
            totalReserved += tcmd.maxChars;
        }

        while (m_textAllocations.size() > totalReserved) {
            ctx.geometry->release(*m_textAllocations.back());
            m_textAllocations.pop_back();
        }

        size_t slot = 0;
        for (const auto &tcmd : m_textCommands) {
            std::string truncated = tcmd.text.substr(0, tcmd.maxChars);

            TextLayoutParams params;
            params.position = absolutePosition + tcmd.position;
            params.bounds = absoluteSize - tcmd.position;
            params.fontSize = tcmd.fontSize;
            params.color = tcmd.color;

            auto glyphs = ctx.textProcessor->layoutTextAtlas(truncated, params);

            for (size_t i = 0; i < glyphs.size(); i++) {
                glyphs[i].clipRect = clipRect;
                glyphs[i].zIndex = getZIndex() + 1;
                glyphs[i].setVisible(isVisible());

                if (slot < m_textAllocations.size()) {
                    ctx.geometry->update(*m_textAllocations[slot], glyphs[i]);
                } else {
                    m_textAllocations.push_back(ctx.geometry->submit(glyphs[i]));
                }
                slot++;
            }

            for (size_t i = glyphs.size(); i < tcmd.maxChars; i++) {
                InstanceData hidden{};
                hidden.setVisible(false);

                if (slot < m_textAllocations.size()) {
                    ctx.geometry->update(*m_textAllocations[slot], hidden);
                } else {
                    m_textAllocations.push_back(ctx.geometry->submit(hidden));
                }
                slot++;
            }
        }
    } else {
        while (!m_textAllocations.empty()) {
            ctx.geometry->release(*m_textAllocations.back());
            m_textAllocations.pop_back();
        }
    }

    flags &= ~FLAG_DIRTY;
}

} // namespace Amethyst
