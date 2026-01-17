/*
 * Scrollable container with scrollbars
 */

#ifndef AMETHYST__SCROLLING_FRAME_H
#define AMETHYST__SCROLLING_FRAME_H

#include "components/common.h"
#include "components/ui_object.h"

namespace Amethyst {

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
    ScrollingFrame() = default;
    ScrollingFrame(Instance *parent) { setParent(parent); };
    virtual ~ScrollingFrame() = default;

    void draw(DrawContext &ctx) override;

  protected:
    void onMouseScrollUp() override;
    void onMouseScrollDown() override;

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
    glm::vec2 m_scrollOffset = {0.0f, 0.0f};
};

} // namespace Amethyst

#endif // AMETHYST__SCROLLING_FRAME_H
