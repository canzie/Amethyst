
#ifndef AMETHYST__PANEL_LAYER_H
#define AMETHYST__PANEL_LAYER_H

#include "components/ui_layer.h"
#include <glm/glm.hpp>

namespace Amethyst {

class PanelLayer : public UILayer {
  public:
    PanelLayer();
    virtual ~PanelLayer();

    void draw(DrawContext &ctx) override;
};

} // namespace Amethyst

#endif // AMETHYST__PANEL_LAYER_H
