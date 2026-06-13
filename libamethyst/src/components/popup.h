/*
 * Popup: a frame rendered in the overlay.
 */

#ifndef AMETHYST__POPUP_H
#define AMETHYST__POPUP_H

#include "components/frame.h"
#include "math/math.h"
#include "modules/event_signal.h"

#include <functional>

namespace Amethyst {

enum class PopupPlacement {
    BELOW,
    ABOVE,
    LEFT,
    RIGHT,
};

class Popup : public Frame {
  public:
    Popup();

    /**
     * @brief Show the popup positioned against a trigger, clamped to stay on screen.
     * @param anchor Object whose absolute rect the popup is placed relative to, per placement
     */
    void open(UIObject *anchor);

    /**
     * @brief Show the popup with its top-left at an absolute point, clamped to stay on screen.
     * @param absolutePoint Top-left position in absolute coordinates
     */
    void openAt(vec2 absolutePoint);

    void close();
    bool isOpen() const { return m_open; }

  public:
    PopupPlacement placement = PopupPlacement::BELOW;
    vec2 offset = vec2(0.0f);
    bool matchAnchorWidth = false;
    bool closeOnClickOutside = true;

    std::function<void()> onOpened;
    std::function<void()> onClosed;

  private:
    void ensureConnected();
    vec2 resolvePlacement(vec2 anchorPos, vec2 anchorSize, vec2 contentSize, vec2 viewport) const;

    bool m_open = false;
    EventConnection m_pressConn;
};

} // namespace Amethyst

#endif // AMETHYST__POPUP_H
