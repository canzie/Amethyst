/*
 * Root container for UI hierarchy
 */

#ifndef AMETHYST__WINDOW_H
#define AMETHYST__WINDOW_H

#include "components/ui_layer.h"

namespace Amethyst {

class Window : public UILayer {
  public:
    Window() = default;
    virtual ~Window() = default;

    void draw(GeometryRegistry& registry) override;

  public:
    uint32_t displayOrder; // kind of like the zindex for windows
};

} // namespace Amethyst

#endif // AMETHYST__WINDOW_H
