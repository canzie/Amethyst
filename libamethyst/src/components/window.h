/*
 * Root container for UI hierarchy
 */

#ifndef AMETHYST__WINDOW_H
#define AMETHYST__WINDOW_H

#include "components/input_events.h"
#include "components/ui_layer.h"
#include <glm/glm.hpp>
#include <vector>

namespace Amethyst {

class Instance;
class UIObject;

class Window : public UILayer {
  public:
    Window();
    virtual ~Window();

    void draw(DrawContext &ctx) override;

    void onMouseButton(int button, int action, int mods, uint32_t x, uint32_t y);
    void onMouseScroll(float xoffset, float yoffset, uint32_t x, uint32_t y);
    void onMouseMove(uint32_t x, uint32_t y);

    void captureMouse(UIObject *object);
    void releaseMouse(UIObject *object);
    UIObject *getMouseCapture() const { return m_mouseCapturedBy; }

  private:
    Instance *findClickedObject(uint32_t x, uint32_t y);
    Instance *findClickedObjectRecursive(const std::vector<Instance *> &instances, const glm::vec2 &point);

  public:
    uint32_t displayOrder;

  private:
    Instance *m_lastHoveredInstance = nullptr;
    UIObject *m_mouseCapturedBy = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__WINDOW_H
