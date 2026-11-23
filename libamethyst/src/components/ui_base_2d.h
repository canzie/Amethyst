/*
 * Base class for all 2D drawable UI elements
 */

#ifndef AMETHYST__UI_BASE_2D_H
#define AMETHYST__UI_BASE_2D_H

#include "components/common.h"
#include "components/instance.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

struct DrawContext;

struct UIBase2DProperties {};

class UIBase2D : public Instance {
  public:
    UIBase2D() = default;
    virtual ~UIBase2D()
    {
        if (m_geometryAlloc && m_geometryAlloc->isValid() && m_geometryAlloc->owning) {
            m_geometryAlloc->registry->release(*m_geometryAlloc);
        }
    }

    virtual void draw(DrawContext &ctx) = 0;

    /**
     * @brief Top-down layout pass.
     */
    virtual void arrange() = 0;

    bool containsPoint(const vec2 &point) const override
    {
        return point.x >= absolutePosition.x && point.x <= absolutePosition.x + absoluteSize.x && point.y >= absolutePosition.y &&
               point.y <= absolutePosition.y + absoluteSize.y;
    }

    bool containsScreenPoint(const vec2 &point) const override { return containsPoint(toLayout(point)); }

    /**
     * @brief Maps a point in this node's layout space to where it is drawn in the window.
     * @param layoutPoint A point in the same space as absolutePosition
     * @return The point in window coordinates
     */
    vec2 toScreen(vec2 layoutPoint) const { return layoutPoint * absoluteScale + screenOffset; }

    /**
     * @brief Maps a point in the window to this node's layout space, the inverse of toScreen().
     * @param screenPoint A point in window coordinates
     * @return The point in the same space as absolutePosition
     */
    vec2 toLayout(vec2 screenPoint) const { return (screenPoint - screenOffset) / absoluteScale; }

    /**
     * @brief Where this node is drawn in the window, the scale of it and its ancestors included.
     * @return The rect as (minX, minY, maxX, maxY) in window coordinates
     */
    vec4 screenRect() const
    {
        vec2 min = toScreen(absolutePosition);
        vec2 max = toScreen(absolutePosition + absoluteSize);
        return {min.x, min.y, max.x, max.y};
    }

    /**
     * @brief Whether this node is drawn scaled, by its own scale or an ancestor's.
     */
    bool isScaled() const { return absoluteScale != 1.0f || screenOffset != vec2(0.0f); }

    void setProperties(UIBase2DProperties props);

    /**
     * @brief Maps an instance built in this node's layout space to where it is drawn in the window.
     * pushData() does this itself; instances written straight into a registry must call it.
     * @param data The instance to map in place
     */
    virtual void mapToScreen(InstanceData &data) const;

  protected:
    /**
     * @brief Takes over the parent's transform unchanged, for nodes that have no scale of their own.
     */
    void inheritTransform();

    /**
     * @brief Writes an instance of this node, mapping it from layout space to the window first.
     * @param registry Registry to write into
     * @param layoutData The instance in this node's layout space
     * @return The node's allocation
     */
    GeometryAllocation *pushData(GeometryRegistry *registry, const InstanceData &layoutData);

  public:
    vec2 absolutePosition = vec2(0.0f);
    vec2 absoluteContentPosition = vec2(0.0f);
    Degrees absoluteRotation = 0.0f;
    // the layout values above are drawn at window = layout * absoluteScale + screenOffset, the
    // product of this node's scale and its ancestors', each around its anchor point
    float absoluteScale = 1.0f;
    vec2 screenOffset = vec2(0.0f);
    vec2 absoluteSize = vec2(0.0f);
    vec2 absoluteContentSize = vec2(0.0f);
    // in window coordinates, since the nodes it clips can be drawn at different scales
    vec4 clipRect = {0.0f, 0.0f, 0.0f, 0.0f};

  protected:
    GeometryAllocation *m_geometryAlloc = nullptr;
    vec4 m_appliedClipRect = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__UI_BASE_2D_H
