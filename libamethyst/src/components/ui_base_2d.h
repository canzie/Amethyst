/*
 * Base class for all 2D drawable UI elements
 */

#ifndef AMETHYST__UI_BASE_2D_H
#define AMETHYST__UI_BASE_2D_H

#include "components/common.h"
#include "components/instance.h"

namespace Amethyst {

class GeometryRegistry;

class UIBase2D : public Instance {
  public:
    UIBase2D() = default;
    virtual ~UIBase2D() = default;

    virtual void draw(GeometryRegistry &registry) = 0;

    bool containsPoint(const glm::vec2 &point) const
    {
        return point.x >= absolutePosition.x && point.x <= absolutePosition.x + absoluteSize.x && point.y >= absolutePosition.y &&
               point.y <= absolutePosition.y + absoluteSize.y;
    }

  public:
    glm::vec2 absolutePosition = glm::vec2(0.0f);
    Degrees absoluteRotation = 0.0f;
    glm::vec2 absoluteSize;

  protected:
    uint32_t m_allocationIndex = UINT32_MAX;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BASE_2D_H
