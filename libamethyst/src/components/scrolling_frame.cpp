#include "components/scrolling_frame.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

static void applyStyle(ScrollingFrame &frame)
{
    const auto &style = Style::instance();
    frame.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::SCROLLING_FRAME);
    frame.backgroundTransparency = style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::SCROLLING_FRAME);
    frame.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::SCROLLING_FRAME);
    frame.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::SCROLLING_FRAME);
    frame.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::SCROLLING_FRAME);
    frame.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::SCROLLING_FRAME);
    frame.scrollBarColor = style.get<Color3>(StyleProperty::SCROLLBAR_COLOR, ComponentType::SCROLLING_FRAME);
    frame.scrollBarTransparency = style.get<float>(StyleProperty::SCROLLBAR_TRANSPARENCY, ComponentType::SCROLLING_FRAME);
    frame.scrollBarThickness = style.get<float>(StyleProperty::SCROLLBAR_THICKNESS, ComponentType::SCROLLING_FRAME);
    frame.scrollBarThumbColor = style.get<Color3>(StyleProperty::SCROLLBAR_THUMB_COLOR, ComponentType::SCROLLING_FRAME);
    frame.scrollBarThumbTransparency = style.get<float>(StyleProperty::SCROLLBAR_THUMB_TRANSPARENCY, ComponentType::SCROLLING_FRAME);
}

ScrollingFrame::ScrollingFrame()
{
    applyStyle(*this);
}

Instance *ScrollingFrame::addChild(std::unique_ptr<Instance> child)
{
    Instance *raw = child.get();
    Instance *result = Instance::addChild(std::move(child));
    if (auto *obj = raw->as<UIObject>()) {
        m_childViewportVisibility[raw] = obj->visible;
    }
    return result;
}

std::unique_ptr<Instance> ScrollingFrame::removeChild(Instance *child)
{
    if (child->as<UIObject>()) {
        m_childViewportVisibility.erase(child);
    }
    return Instance::removeChild(child);
}

void ScrollingFrame::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    glm::vec2 absCanvasSize = canvasSize.resolve(absoluteSize);
    glm::vec2 maxScroll = glm::max(absCanvasSize - absoluteSize, glm::vec2(0.0f));

    m_scrollOffset = glm::clamp(m_scrollOffset, glm::vec2(0.0f), maxScroll);

    if (auto *gridLayout = getExtension<UIGridLayout>()) {
        gridLayout->apply(m_children);
    } else if (auto *listLayout = getExtension<UIListLayout>()) {
        listLayout->apply(m_children);
    }

    glm::vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        auto *obj = child->as<UIObject>();
        if (obj == nullptr) continue;

        glm::vec2 childPos = obj->position.resolve(absCanvasSize) - m_scrollOffset;
        glm::vec2 childSize = obj->size.resolve(absCanvasSize);

        bool inViewport = (childPos.x + childSize.x > 0.0f) && (childPos.x < absoluteSize.x) && (childPos.y + childSize.y > 0.0f) &&
                          (childPos.y < absoluteSize.y);

        bool original = obj->visible;
        bool effective = original && inViewport;

        auto &lastViewport = m_childViewportVisibility[child.get()];
        if (effective != lastViewport) {
            obj->markDirty();
            lastViewport = effective;
        }

        obj->visible = effective;
        obj->clipRect = childClip;
        obj->computeAbsolutes(absCanvasSize, absolutePosition - m_scrollOffset, absoluteRotation);
        obj->draw(ctx);
        obj->visible = original;
    }

    drawScrollbars(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void ScrollingFrame::drawScrollbars(DrawContext &ctx)
{
    glm::vec2 absCanvasSize = canvasSize.resolve(absoluteSize);
    bool needsVertical = (scrollAxis == ScrollAxis::Y || scrollAxis == ScrollAxis::XY) && absCanvasSize.y > absoluteSize.y;
    bool needsHorizontal = (scrollAxis == ScrollAxis::X || scrollAxis == ScrollAxis::XY) && absCanvasSize.x > absoluteSize.x;

    if (scrollBarVisibility == ScrollBarVisibility::NEVER) {
        needsVertical = false;
        needsHorizontal = false;
    }

    if (needsVertical) {
        if (m_verticalBar == nullptr) {
            m_verticalBar = std::make_unique<Frame>();
            m_verticalBar->zIndex = getZIndex() + 1;
            m_verticalBar->size = UDim2::fromScale(1.0f, 1.0f);
        }
        if (m_verticalThumb == nullptr) {
            m_verticalThumb = std::make_unique<Frame>();
            m_verticalThumb->zIndex = getZIndex() + 2;
            m_verticalThumb->size = UDim2::fromScale(1.0f, 1.0f);
        }

        float trackHeight = absoluteSize.y - (needsHorizontal ? scrollBarThickness : 0.0f);
        float thumbRatio = absoluteSize.y / absCanvasSize.y;
        float thumbHeight = trackHeight * thumbRatio;
        float scrollRatio = m_scrollOffset.y / (absCanvasSize.y - absoluteSize.y);
        float thumbY = scrollRatio * (trackHeight - thumbHeight);

        m_verticalBar->backgroundColor = scrollBarColor;
        m_verticalBar->backgroundTransparency = scrollBarTransparency;
        m_verticalBar->markDirty();
        m_verticalBar->computeAbsolutes(glm::vec2(scrollBarThickness, trackHeight),
                                        absolutePosition + glm::vec2(absoluteSize.x - scrollBarThickness, 0.0f), absoluteRotation);
        m_verticalBar->draw(ctx);

        m_verticalThumb->backgroundColor = scrollBarThumbColor;
        m_verticalThumb->backgroundTransparency = scrollBarThumbTransparency;
        m_verticalThumb->markDirty();
        m_verticalThumb->computeAbsolutes(glm::vec2(scrollBarThickness, thumbHeight),
                                          absolutePosition + glm::vec2(absoluteSize.x - scrollBarThickness, thumbY),
                                          absoluteRotation);
        m_verticalThumb->draw(ctx);
    } else {
        m_verticalBar.reset();
        m_verticalThumb.reset();
    }

    if (needsHorizontal) {
        if (m_horizontalBar == nullptr) {
            m_horizontalBar = std::make_unique<Frame>();
            m_horizontalBar->zIndex = getZIndex() + 1;
            m_horizontalBar->size = UDim2::fromScale(1.0f, 1.0f);
        }
        if (m_horizontalThumb == nullptr) {
            m_horizontalThumb = std::make_unique<Frame>();
            m_horizontalThumb->zIndex = getZIndex() + 2;
            m_horizontalThumb->size = UDim2::fromScale(1.0f, 1.0f);
        }

        float trackWidth = absoluteSize.x - (needsVertical ? scrollBarThickness : 0.0f);
        float thumbRatio = absoluteSize.x / absCanvasSize.x;
        float thumbWidth = trackWidth * thumbRatio;
        float scrollRatio = m_scrollOffset.x / (absCanvasSize.x - absoluteSize.x);
        float thumbX = scrollRatio * (trackWidth - thumbWidth);

        m_horizontalBar->backgroundColor = scrollBarColor;
        m_horizontalBar->backgroundTransparency = scrollBarTransparency;
        m_horizontalBar->markDirty();
        m_horizontalBar->computeAbsolutes(glm::vec2(trackWidth, scrollBarThickness),
                                          absolutePosition + glm::vec2(0.0f, absoluteSize.y - scrollBarThickness),
                                          absoluteRotation);
        m_horizontalBar->draw(ctx);

        m_horizontalThumb->backgroundColor = scrollBarThumbColor;
        m_horizontalThumb->backgroundTransparency = scrollBarThumbTransparency;
        m_horizontalThumb->markDirty();
        m_horizontalThumb->computeAbsolutes(glm::vec2(thumbWidth, scrollBarThickness),
                                            absolutePosition + glm::vec2(thumbX, absoluteSize.y - scrollBarThickness),
                                            absoluteRotation);
        m_horizontalThumb->draw(ctx);
    } else {
        m_horizontalBar.reset();
        m_horizontalThumb.reset();
    }
}

bool ScrollingFrame::onMouseScrollUp()
{
    if (scrollAxis == ScrollAxis::Y || scrollAxis == ScrollAxis::XY) {
        m_scrollOffset.y -= scrollSpeed;
    } else {
        m_scrollOffset.x -= scrollSpeed;
    }
    markDirty();
    return true;
}

bool ScrollingFrame::onMouseScrollDown()
{
    if (scrollAxis == ScrollAxis::Y || scrollAxis == ScrollAxis::XY) {
        m_scrollOffset.y += scrollSpeed;
    } else {
        m_scrollOffset.x += scrollSpeed;
    }
    markDirty();
    return true;
}

} // namespace Amethyst
