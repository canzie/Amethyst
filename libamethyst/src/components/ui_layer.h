/*
 * Container for grouping UI elements
 */

#ifndef AMETHYST__UI_LAYER_H
#define AMETHYST__UI_LAYER_H

#include "components/common.h"
#include "components/ui_base_2d.h"

namespace Amethyst {

class UILayer : public UIBase2D {
  public:
    UILayer() = default;
    virtual ~UILayer() = default;

    void draw(GeometryRegistry& registry) override;

  public:
    bool resetOnSpawn = false;
    ZIndexBehavior zindexBehavior;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LAYER_H
