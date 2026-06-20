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
    virtual ~ScrollingFrame() = default;

    void draw(DrawContext &ctx) override;
    void computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation) override;
    void resolveStyle() override;
    Instance *addChild(std::unique_ptr<Instance> child) override;
    std::unique_ptr<Instance> removeChild(Instance *child) override;

    const ScrollingFrameStyleProperties &getScrollingFrameProperties() { return m_sfProps; }
    bool setScrollingFrameProperties(const ScrollingFrameStyleProperties &props);

  protected:
    EventResult onMouseScrollUp() override;
    EventResult onMouseScrollDown() override;

  protected:
    ScrollingFrameStyleProperties m_sfProps;

  private:
    void drawScrollbars(DrawContext &ctx, vec2 absCanvasSize, vec2 viewport, bool needsVertical, bool needsHorizontal);

    vec2 m_scrollOffset = {0.0f, 0.0f};
    vec2 m_maxScroll = {0.0f, 0.0f};
    std::unique_ptr<Frame> m_verticalBar;
    std::unique_ptr<Frame> m_verticalThumb;
    std::unique_ptr<Frame> m_horizontalBar;
    std::unique_ptr<Frame> m_horizontalThumb;

    std::unordered_map<Instance *, bool> m_childViewportVisibility;
};

} // namespace Amethyst

#endif // AMETHYST__SCROLLING_FRAME_H
