/**
 * @file canvas.h
 * @brief Immediate-mode drawing surface for arbitrary primitives
 *
 * Canvas provides a way to draw lines, triangles, circles, and other primitives
 * that cannot be efficiently rendered with SDF-based approaches. Useful for
 * gizmos, graphs, custom shapes, and other dynamic drawing needs.
 *
 * Unlike other UIObjects which use the SDF pipeline, Canvas batches vertex data
 * and renders using a simple vertex-colored pipeline.
 */

#ifndef AMETHYST__CANVAS_H
#define AMETHYST__CANVAS_H

#include "components/common.h"
#include "components/instance.h"
#include "components/ui_object.h"

#include <cstdint>
#include <vector>

namespace Amethyst {

/**
 * @brief Vertex data for canvas primitives
 */
struct CanvasVertex {
    glm::vec2 position;
    Color4 color;
};

/**
 * @brief Types of primitives that can be drawn on a Canvas
 */
enum class CanvasPrimitiveType : uint8_t {
    LINE,
    TRIANGLE,
    TRIANGLE_STRIP,
    CIRCLE,
    RECTANGLE
};

/**
 * @brief Stored primitive command for batched rendering
 */
struct CanvasPrimitive {
    CanvasPrimitiveType type;
    uint32_t vertexOffset;
    uint32_t vertexCount;
    uint32_t indexOffset;
    uint32_t indexCount;
    float thickness;
    bool filled;
};

/**
 * @brief Immediate-mode drawing surface for arbitrary primitives
 *
 * Canvas collects draw commands during the frame and batches them into
 * vertex/index buffers for efficient rendering. The canvas is cleared
 * each frame before draw() is called, so all primitives must be
 * resubmitted every frame.
 *
 * Example usage:
 * @code
 * canvas->clear();
 * canvas->drawLine({0, 0}, {100, 100}, {1, 0, 0, 1}, 2.0f);
 * canvas->drawTriangleFilled({50, 0}, {0, 100}, {100, 100}, {0, 1, 0, 1});
 * canvas->drawCircle({50, 50}, 25.0f, {0, 0, 1, 1}, 16);
 * @endcode
 */
class Canvas : public UIObject {
  public:
    Canvas() = default;
    Canvas(Instance *parent);
    virtual ~Canvas() = default;

    void draw(DrawContext &ctx) override;

    /**
     * @brief Clear all primitives from the canvas
     *
     * Should be called at the start of each frame before submitting new
     * draw commands. If not called, primitives accumulate across frames.
     */
    void clear();

    /**
     * @brief Draw a line between two points
     * @param start Starting point in local coordinates
     * @param end Ending point in local coordinates
     * @param color Line color with alpha
     * @param thickness Line thickness in pixels
     */
    void drawLine(glm::vec2 start, glm::vec2 end, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw a series of connected lines
     * @param points Vector of points to connect
     * @param color Line color with alpha
     * @param thickness Line thickness in pixels
     * @param closed If true, connect last point back to first
     */
    void drawPolyline(const std::vector<glm::vec2> &points, Color4 color, float thickness = 1.0f, bool closed = false);

    /**
     * @brief Draw a filled triangle
     */
    void drawTriangleFilled(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color);

    /**
     * @brief Draw a triangle outline
     */
    void drawTriangleStroke(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw a filled rectangle
     */
    void drawRectangleFilled(glm::vec2 topLeft, glm::vec2 size, Color4 color);

    /**
     * @brief Draw a rectangle outline
     */
    void drawRectangleStroke(glm::vec2 topLeft, glm::vec2 size, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw a filled circle
     * @param segments Number of segments (higher = smoother)
     */
    void drawCircleFilled(glm::vec2 center, float radius, Color4 color, uint32_t segments = 32);

    /**
     * @brief Draw a circle outline
     * @param segments Number of segments (higher = smoother)
     */
    void drawCircleStroke(glm::vec2 center, float radius, Color4 color, float thickness = 1.0f, uint32_t segments = 32);

    /**
     * @brief Draw a filled arc (pie slice)
     * @param startAngle Starting angle in radians
     * @param endAngle Ending angle in radians
     */
    void drawArcFilled(glm::vec2 center, float radius, float startAngle, float endAngle, Color4 color, uint32_t segments = 32);

    /**
     * @brief Draw an arc outline
     */
    void drawArcStroke(glm::vec2 center, float radius, float startAngle, float endAngle, Color4 color, float thickness = 1.0f,
                       uint32_t segments = 32);

    /**
     * @brief Draw a filled convex polygon
     * @param points Vertices of the polygon (must be convex, wound counter-clockwise)
     */
    void drawConvexPolygonFilled(const std::vector<glm::vec2> &points, Color4 color);

    /**
     * @brief Draw a polygon outline
     */
    void drawPolygonStroke(const std::vector<glm::vec2> &points, Color4 color, float thickness = 1.0f);

    /**
     * @brief Draw a cubic bezier curve
     * @param segments Number of line segments to approximate the curve
     */
    void drawBezierCubic(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color4 color, float thickness = 1.0f,
                         uint32_t segments = 32);

    /**
     * @brief Draw a quadratic bezier curve
     * @param segments Number of line segments to approximate the curve
     */
    void drawBezierQuadratic(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, Color4 color, float thickness = 1.0f,
                             uint32_t segments = 32);

    const std::vector<CanvasVertex> &getVertices() const { return m_vertices; }
    const std::vector<uint32_t> &getIndices() const { return m_indices; }
    const std::vector<CanvasPrimitive> &getPrimitives() const { return m_primitives; }
    bool isEmpty() const { return m_primitives.empty(); }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

  private:
    uint32_t addVertices(const std::vector<CanvasVertex> &vertices);
    uint32_t addIndices(const std::vector<uint32_t> &indices);
    void generateLineGeometry(glm::vec2 start, glm::vec2 end, Color4 color, float thickness);

  private:
    std::vector<CanvasVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<CanvasPrimitive> m_primitives;
    bool m_dirty = false;
};

} // namespace Amethyst

#endif // AMETHYST__CANVAS_H
