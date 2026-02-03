/*
 * Scrollable container with scrollbars
 */

#ifndef AMETHYST__SCROLLING_FRAME_H
#define AMETHYST__SCROLLING_FRAME_H

#include "components/common.h"
#include "components/ui_object.h"
#include <memory>

namespace Amethyst {

class Frame;

enum class ScrollBarVisibility {
    ALWAYS,
    AUTO,
    NEVER
};

enum class ScrollAxis {
    X,
    Y,
    XY
};

class ScrollingFrame : public UIObject {
  public:
    ScrollingFrame();
    ScrollingFrame(Instance *parent);
    virtual ~ScrollingFrame() = default;

    void draw(DrawContext &ctx) override;
    void addChild(Instance *child) override;
    void removeChild(Instance *child) override;

  protected:
    bool onMouseScrollUp() override;
    bool onMouseScrollDown() override;

  public:
    ScrollAxis scrollAxis = ScrollAxis::Y;
    ScrollBarVisibility scrollBarVisibility = ScrollBarVisibility::AUTO;

    UDim2 canvasSize;
    UDim2 canvasPosition;

    Color3 scrollBarColor = {0.7f, 0.7f, 0.7f};
    float scrollBarTransparency = 0.0f;
    float scrollBarThickness = 8.0f;

    Color3 scrollBarThumbColor = {0.5f, 0.5f, 0.5f};
    float scrollBarThumbTransparency = 0.0f;

    float scrollSpeed = 20.0f;
    bool elasticScrolling = false;

  private:
    void drawScrollbars(DrawContext &ctx);

    glm::vec2 m_scrollOffset = {0.0f, 0.0f};
    std::unique_ptr<Frame> m_verticalBar;
    std::unique_ptr<Frame> m_verticalThumb;
    std::unique_ptr<Frame> m_horizontalBar;
    std::unique_ptr<Frame> m_horizontalThumb;

    std::unordered_map<Instance *, bool> m_childViewportVisibility;
};

} // namespace Amethyst

#endif // AMETHYST__SCROLLING_FRAME_H
