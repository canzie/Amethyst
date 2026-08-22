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

    void setProperties(UIBase2DProperties props);

  protected:
    GeometryAllocation *pushData(GeometryRegistry *registry, const InstanceData &data);

  public:
    vec2 absolutePosition = vec2(0.0f);
    vec2 absoluteContentPosition = vec2(0.0f);
    Degrees absoluteRotation = 0.0f;
    vec2 absoluteSize = vec2(0.0f);
    vec2 absoluteContentSize = vec2(0.0f);
    vec4 clipRect = {0.0f, 0.0f, 0.0f, 0.0f};

  protected:
    GeometryAllocation *m_geometryAlloc = nullptr;
    vec4 m_appliedClipRect = {0.0f, 0.0f, 0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__UI_BASE_2D_H
