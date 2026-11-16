/*
 * Root container for UI hierarchy
 */

#ifndef AMETHYST__WINDOW_H
#define AMETHYST__WINDOW_H

#include "components/input_events.h"
#include "components/overlay_layer.h"
#include "components/ui_layer.h"
#include "math/math.h"
#include "modules/event_signal.h"
#include "utils/free_list.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>

namespace Amethyst {

class Instance;
class TooltipStack;
class UIObject;
class Window;

/**
 * @brief Opaque handle to a tick subscription; carries its own window so the holder need not.
 */
struct TickHandle {
    Window *window = nullptr;
    uint32_t id = 0;

    bool active() const { return window != nullptr; }

    /**
     * @brief Drop the subscription and clear the handle. Safe to call when inactive.
     */
    void unregister();
};

class Window : public UILayer {
  public:
    Window();
    virtual ~Window();

    void draw(DrawContext &ctx) override;
    void arrange() override;

    void onMouseButton(int button, int action, int mods, int32_t x, int32_t y);
    void onMouseScroll(float xoffset, float yoffset, int32_t x, int32_t y);
    void onMouseMove(int32_t x, int32_t y);

    void captureMouse(UIObject *object);
    void releaseMouse(UIObject *object);
    UIObject *getMouseCapture() const { return m_mouseCapturedBy; }

    OverlayLayer *getOverlayLayer() { return m_overlayLayer.get(); }
    TooltipStack &getTooltips() { return *m_tooltips; }
    std::vector<Instance *> getHittableInstances() override;

    /**
     * @brief Subscribe a callback to per-frame ticks. Pair with unregisterTick on teardown.
     * @param callback Invoked with the frame delta each time tick() runs
     * @return Handle used to unsubscribe
     */
    TickHandle registerTick(std::function<void(float)> callback);

    /**
     * @brief Remove a tick subscription.
     * @param id Id from the handle returned by registerTick
     */
    void unregisterTick(uint32_t id);

    /**
     * @brief Invoke every registered tick callback. Call once per frame.
     * @param deltaTime Seconds since the previous tick
     */
    void tick(float deltaTime);

  private:
    Instance *findClickedObject(int32_t x, int32_t y);
    void updateHover(int32_t x, int32_t y);

  private:
    static constexpr uint8_t MAX_HOVER_DEPTH = 16;

    // Hovered nodes are held weakly: an enter/leave handler may destroy any of them, and a
    // destroyed node must neither get further events nor leave a subscription behind.
    struct HoverEntry {
        UIObject *object = nullptr;
        std::weak_ptr<bool> alive;
    };

    struct HoverStack {
        std::array<HoverEntry, MAX_HOVER_DEPTH> items;
        uint8_t count = 0;
    };

    HoverStack m_hoverCurrent;
    HoverStack m_hoverPrevious;
    // The node that consumed each button's press, so it can be told when the release lands
    // elsewhere instead of being left in its pressed state.
    struct PressRecord {
        UIObject *object = nullptr;
        std::weak_ptr<bool> alive;
        bool released = false;
    };
    std::array<PressRecord, 3> m_pressTargets;

    vec2 m_lastMousePosition = vec2(0.0f);
    bool m_hasMousePosition = false;
    bool m_hoverStale = false;
    UIObject *m_mouseCapturedBy = nullptr;
    EventConnection m_mouseCapturedByConn;
    std::unique_ptr<OverlayLayer> m_overlayLayer;
    std::unique_ptr<TooltipStack> m_tooltips;
    FreeList<std::function<void(float)>> m_tickCallbacks;
};

} // namespace Amethyst

#endif // AMETHYST__WINDOW_H
