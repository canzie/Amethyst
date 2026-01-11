/*
 * Base class for non-renderable UI components (layouts, constraints)
 */

#ifndef AMETHYST__UI_COMPONENT_H
#define AMETHYST__UI_COMPONENT_H

#include "components/instance.h"

namespace Amethyst {

class UIComponent : public Instance {
  public:
    UIComponent() = default;
    virtual ~UIComponent() = default;

    virtual void apply() = 0;

  public:
    bool enabled = true;
};

} // namespace Amethyst

#endif // AMETHYST__UI_COMPONENT_H
