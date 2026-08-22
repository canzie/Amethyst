/*
 * Scrollable container with scrollbars
 */

#ifndef AMETHYST__SCROLLING_FRAME_H
#define AMETHYST__SCROLLING_FRAME_H

#include "components/common.h"
#include "components/properties.h"
#include "components/ui_object.h"
#include <memory>

namespace Amethyst {

class Frame;

class ScrollingFrame : public UIObject {
  public:
    ScrollingFrame();
    virtual ~ScrollingFrame();

    void draw(DrawContext &ctx) override;
    void arrange() override;
    void resolveStyle() override;

    const ScrollingFrameStyleProperties &getScrollingFrameProperties() { return m_sfProps; }
    bool setScrollingFrameProperties(const ScrollingFrameStylePropertiesArgs &props);

    /**
     * @brief Current scroll position, normalized to [0, 1] per axis (0 = start, 1 = fully scrolled).
     * @return The normalized scroll position; 0 on an axis with nothing to scroll
     */
    vec2 getScrollFraction() const;

    /**
     * @brief Maximum scroll offset in canvas pixels, i.e. canvas size minus viewport size (clamped to 0).
     * @return The per-axis pixel scroll range, as of the last arrange()
     */
    vec2 getMaxScroll() const { return m_maxScroll; }

    /**
     * @brief Current scroll offset in canvas pixels.
     * @return The scroll offset, clamped to [0, getMaxScroll()] as of the last arrange()
     */
    vec2 getScrollOffset() const { return m_scrollOffset; }

  protected:
    EventResult onMouseScrollUp() override;
    EventResult onMouseScrollDown() override;

  protected:
    ScrollingFrameStyleProperties m_sfProps;

  private:
    void layoutChildren(vec2 &absCanvasSize, vec2 viewport);

    /**
     * @brief Brings the scroll offset back inside the range the current canvas allows
     * @return Whether the offset had to move
     */
    bool clampScrollOffset();
    void arrangeScrollbars();
    void drawScrollbars(DrawContext &ctx);

    vec2 m_scrollOffset = {0.0f, 0.0f};
    vec2 m_maxScroll = {0.0f, 0.0f};
    vec2 m_absCanvasSize = {0.0f, 0.0f};
    bool m_needsVertical = false;
    bool m_needsHorizontal = false;
    std::unique_ptr<Frame> m_verticalBar;
    std::unique_ptr<Frame> m_verticalThumb;
    std::unique_ptr<Frame> m_horizontalBar;
    std::unique_ptr<Frame> m_horizontalThumb;
};

} // namespace Amethyst

#endif // AMETHYST__SCROLLING_FRAME_H
