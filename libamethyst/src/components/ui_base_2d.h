/*
 * Base class for all 2D drawable UI elements
 */

#ifndef AMETHYST__UI_BASE_2D_H
#define AMETHYST__UI_BASE_2D_H

#include "components/common.h"
#include "components/instance.h"

namespace Amethyst {

struct DrawContext;
struct GeometryAllocation;

class UIBase2D : public Instance {
  public:
    UIBase2D() = default;
    virtual ~UIBase2D() {
        // TODO: find a way to free any allocaitons
        // could maybe make the window own the drawcontext and go up to the window in here? idk
    };

    virtual void draw(DrawContext &ctx) = 0;

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
    GeometryAllocation *m_geometryAlloc = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BASE_2D_H
