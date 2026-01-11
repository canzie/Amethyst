/*
 * Base class for all 2D drawable UI elements
 */

#ifndef AMETHYST__UI_BASE_2D_H
#define AMETHYST__UI_BASE_2D_H

#include "components/common.h"
#include "components/instance.h"

namespace Amethyst {

class UIBase2D : public Instance {
  public:
    UIBase2D() = default;
    virtual ~UIBase2D() = default;

    virtual void draw() = 0;

  public:
    glm::vec2 absolutePosition;
    Degrees absoluteRotation;
    glm::vec2 absoluteSize;
};

} // namespace Amethyst

#endif // AMETHYST__UI_BASE_2D_H
