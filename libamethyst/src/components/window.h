/*
 * Root container for UI hierarchy
 */

#ifndef AMETHYST__WINDOW_H
#define AMETHYST__WINDOW_H

#include "components/input_events.h"
#include "components/ui_layer.h"
#include <glm/glm.hpp>

namespace Amethyst {

class Instance;

class Window : public UILayer {
  public:
    Window();
    virtual ~Window();

    void draw(GeometryRegistry &registry) override;

    void onMouseButton(int button, int action, int mods, uint32_t x, uint32_t y);
    void onMouseScroll(float xoffset, float yoffset, uint32_t x, uint32_t y);

  private:
    Instance *findClickedObject(uint32_t x, uint32_t y);

  public:
    uint32_t displayOrder;
};

} // namespace Amethyst

#endif // AMETHYST__WINDOW_H
