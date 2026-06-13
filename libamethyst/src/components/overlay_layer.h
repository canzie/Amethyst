
#ifndef AMETHYST__OVERLAY_LAYER_H
#define AMETHYST__OVERLAY_LAYER_H

#include "components/common.h"
#include "components/ui_layer.h"
#include "math/math.h"
#include "modules/event_signal.h"

namespace Amethyst {

/**
 * @brief Accumulates listener votes for a press; propagate wins, and the default with no votes is propagate.
 */
struct PressVote {
    void add(EventResult r)
    {
        if (r == EventResult::PROPAGATE) {
            m_propagate = true;
        } else {
            m_consumed = true;
        }
    }

    EventResult result() const { return (m_propagate || !m_consumed) ? EventResult::PROPAGATE : EventResult::CONSUMED; }

  private:
    bool m_propagate = false;
    bool m_consumed = false;
};

class OverlayLayer : public UILayer {
  public:
    OverlayLayer();
    virtual ~OverlayLayer();

    void draw(DrawContext &ctx) override;

    bool containsPoint(const vec2 &) const override { return false; }

  public:
    EventSignal<void(vec2, PressVote &)> onPressVote;
};

} // namespace Amethyst

#endif // AMETHYST__OVERLAY_LAYER_H
