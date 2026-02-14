/**
 * @file canvas.h
 * @brief Immediate-mode drawing surface for arbitrary primitives
 *
 * Canvas provides a way to draw lines, triangles, quads, and circles
 * using the SDF pipeline. Points are specified in local canvas coordinates
 * and packed into InstanceData::shapeData as half-floats.
 *
 * Each draw call becomes one GPU instance. The canvas is cleared
 * each frame before draw() is called, so all primitives must be
 * resubmitted every frame.
 */

#ifndef AMETHYST__CANVAS_H
#define AMETHYST__CANVAS_H

#include "components/common.h"
#include "components/ui_object.h"

#include <string>
#include <vector>

namespace Amethyst {

class GeometryRegistry;
struct GeometryAllocation;

class Canvas : public UIObject {
  public:
    Canvas() = default;
    ~Canvas();

    void draw(DrawContext &ctx) override;

    /**
     * @brief Clear all primitives and mark dirty.
     * Call at the start of each frame before submitting new draw commands.
     */
    void clear();

    /**
     * @brief Draw a line segment (rendered as a capsule with the given thickness)
     * @param start Start point in local canvas coordinates
     * @param end End point in local canvas coordinates
     * @param color Line color
     * @param thickness Line thickness in pixels
     */
    void drawLine(glm::vec2 start, glm::vec2 end, Color4 color, float thickness = 1.0f);

    void drawTriangleFilled(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color);
    void drawTriangleStroke(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw an arbitrary quadrilateral (4 arbitrary points, wound counter-clockwise)
     */
    void drawQuadFilled(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color4 color);
    void drawQuadStroke(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color4 color, float thickness = 1.0f);

    void drawCircleFilled(glm::vec2 center, float radius, Color4 color);
    void drawCircleStroke(glm::vec2 center, float radius, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw an ellipse outline (ring). The ellipse is axis-aligned in canvas space;
     *        use rotation on the InstanceData level if you need a rotated ellipse.
     * @param center Center in local canvas coordinates
     * @param semiMajor Half-width along X
     * @param semiMinor Half-height along Y
     * @param rotationDeg Rotation of the ellipse in degrees
     * @param color Stroke color
     * @param thickness Stroke width in pixels
     */
    void drawEllipseStroke(glm::vec2 center, float semiMajor, float semiMinor, float rotationDeg, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw text at a position in local canvas coordinates
     * @param text The string to render
     * @param position Top-left position in local canvas coordinates
     * @param fontSize Font size in pixels
     * @param color Text color
     * @param maxChars Maximum characters to reserve GPU slots for.
     *        Text is truncated if it exceeds this. Slots are pre-allocated
     *        so changing the text content doesn't cause reallocation.
     */
    void drawText(const std::string &text, glm::vec2 position, float fontSize, Color4 color, uint32_t maxChars);

    bool isEmpty() const { return m_commands.empty() && m_textCommands.empty(); }

  private:
    struct DrawCmd {
        PrimitiveType primitive;
        glm::vec2 points[4];
        float radius;
        float semiMajor;
        float semiMinor;
        float rotationDeg;
        Color4 fillColor;
        Color4 borderColor;
        float borderThickness;
    };

    struct TextCmd {
        std::string text;
        glm::vec2 position;
        float fontSize;
        Color4 color;
        uint32_t maxChars;
    };

    InstanceData buildInstanceData(const DrawCmd &cmd) const;

    GeometryRegistry *m_registry = nullptr;
    std::vector<DrawCmd> m_commands;
    std::vector<GeometryAllocation *> m_allocations;

    std::vector<TextCmd> m_textCommands;
    std::vector<GeometryAllocation *> m_textAllocations;
};

} // namespace Amethyst

#endif // AMETHYST__CANVAS_H
