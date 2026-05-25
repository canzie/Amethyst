
#ifndef AMETHYST__OVERLAY_LAYER_H
#define AMETHYST__OVERLAY_LAYER_H

#include "components/ui_layer.h"
#include <glm/glm.hpp>

namespace Amethyst {

class OverlayLayer : public UILayer {
  public:
    OverlayLayer();
    virtual ~OverlayLayer();

    void draw(DrawContext &ctx) override;

    bool containsPoint(const glm::vec2 &) const override { return false; }
};

} // namespace Amethyst

#endif // AMETHYST__OVERLAY_LAYER_H
