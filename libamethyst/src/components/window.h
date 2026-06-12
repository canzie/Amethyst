/*
 * Root container for UI hierarchy
 */

#ifndef AMETHYST__WINDOW_H
#define AMETHYST__WINDOW_H

#include "components/input_events.h"
#include "components/overlay_layer.h"
#include "components/ui_layer.h"
#include "math/math.h"
#include <array>
#include <cstdint>
#include <memory>

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

    OverlayLayer *getOverlayLayer() { return m_overlayLayer.get(); }
    std::vector<Instance *> getHittableInstances() override;

  private:
    Instance *findClickedObject(uint32_t x, uint32_t y);
    void purgeFromHoverStacks(Instance *dead);

  private:
    static constexpr uint8_t MAX_HOVER_DEPTH = 16;

    struct HoverStack {
        std::array<UIObject *, MAX_HOVER_DEPTH> items;
        uint8_t count = 0;
    };

    HoverStack m_hoverCurrent;
    HoverStack m_hoverPrevious;
    UIObject *m_mouseCapturedBy = nullptr;
    std::unique_ptr<OverlayLayer> m_overlayLayer;
};

} // namespace Amethyst

#endif // AMETHYST__WINDOW_H
