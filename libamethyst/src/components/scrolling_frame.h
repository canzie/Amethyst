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

  protected:
    EventResult onMouseScrollUp() override;
    EventResult onMouseScrollDown() override;

  protected:
    ScrollingFrameStyleProperties m_sfProps;

  private:
    void layoutChildren(vec2 &absCanvasSize, vec2 viewport);
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
