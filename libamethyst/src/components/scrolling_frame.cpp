#include "components/scrolling_frame.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

ScrollingFrame::ScrollingFrame()
{
    m_sfProps.scrollAxis = ScrollAxis::Y;
    m_sfProps.scrollBarVisibility = ScrollBarVisibility::AUTO;
    m_sfProps.canvasSize = UDim2::fromScale(1.0f, 1.0f);
    m_sfProps.canvasPosition = UDim2::fromOffset(0.0f, 0.0f);
    m_sfProps.scrollBarColor = Color3{0.5f, 0.5f, 0.5f};
    m_sfProps.scrollBarTransparency = 0.0f;
    m_sfProps.scrollBarThickness = 14.0f;
    m_sfProps.scrollBarThumbColor = Color3{0.7f, 0.7f, 0.7f};
    m_sfProps.scrollBarThumbTransparency = 0.0f;
    m_sfProps.scrollSpeed = 30.0f;
    m_sfProps.elasticScrolling = 0;

    resolveStyle();
}

void ScrollingFrame::resolveStyle()
{
    auto &style = Style::instance();
    setBaseStyleProperties(style.getBaseStyle(ComponentType::SCROLLING_FRAME, getClasses()));
    setScrollingFrameProperties(style.getScrollingFrameStyle(ComponentType::SCROLLING_FRAME, getClasses()));
}

bool ScrollingFrame::setScrollingFrameProperties(const ScrollingFrameStyleProperties &props)
{
    bool changed = m_sfProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

Instance *ScrollingFrame::addChild(std::unique_ptr<Instance> child)
{
    Instance *raw = child.get();
    Instance *result = Instance::addChild(std::move(child));
    if (auto *obj = raw->as<UIObject>()) {
        m_childViewportVisibility[raw] = obj->isVisible();
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

    glm::vec2 absCanvasSize = m_sfProps.canvasSize.resolve(absoluteSize);
    glm::vec2 maxScroll = glm::max(absCanvasSize - absoluteSize, glm::vec2(0.0f));

    m_scrollOffset = glm::clamp(m_scrollOffset, glm::vec2(0.0f), maxScroll);

    if (auto *gridLayout = getExtension<UIGridLayout>()) {
        gridLayout->apply(m_children);
    } else if (auto *listLayout = getExtension<UIListLayout>()) {
        listLayout->apply(m_children);
    }

    glm::vec4 childClip = computeChildClipRect();

    glm::vec2 canvasOrigin = m_sfProps.canvasPosition.resolve(absCanvasSize);

    for (auto &child : m_children) {
        auto *obj = child->as<UIObject>();
        if (obj == nullptr) continue;

        glm::vec2 childRelPos = obj->getBaseProperties().position.resolve(absCanvasSize);
        glm::vec2 childEffectivePos = canvasOrigin + childRelPos - m_scrollOffset;
        glm::vec2 childSize = obj->getBaseProperties().size.resolve(absCanvasSize);

        bool inViewport = (childEffectivePos.x + childSize.x > 0.0f) && (childEffectivePos.x < absoluteSize.x) &&
                          (childEffectivePos.y + childSize.y > 0.0f) && (childEffectivePos.y < absoluteSize.y);

        bool original = obj->isVisible();
        bool effective = original && inViewport;

        auto &lastViewport = m_childViewportVisibility[child.get()];
        if (effective != lastViewport) {
            obj->markDirty();
            lastViewport = effective;
        }

        obj->setBaseProperties({.visible = static_cast<int8_t>(effective)});
        obj->clipRect = childClip;
        obj->computeAbsolutes(absCanvasSize, absolutePosition - m_scrollOffset, absoluteRotation);
        obj->draw(ctx);
        obj->setBaseProperties({.visible = static_cast<int8_t>(original)});
    }

    drawScrollbars(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void ScrollingFrame::drawScrollbars(DrawContext &ctx)
{
    bool sfVisible = isVisible();

    glm::vec2 absCanvasSize = m_sfProps.canvasSize.resolve(absoluteSize);
    bool needsVertical = sfVisible && (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                         absCanvasSize.y > absoluteSize.y;
    bool needsHorizontal = sfVisible && (m_sfProps.scrollAxis == ScrollAxis::X || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                           absCanvasSize.x > absoluteSize.x;

    if (m_sfProps.scrollBarVisibility == ScrollBarVisibility::NEVER) {
        needsVertical = false;
        needsHorizontal = false;
    }

    if (needsVertical) {
        if (m_verticalBar == nullptr) {
            m_verticalBar = std::make_unique<Frame>();
            m_verticalBar->name = "vertical bar";
            m_verticalBar->setBaseProperties({.zIndex = getZIndex() + 1});
            m_verticalBar->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }
        if (m_verticalThumb == nullptr) {
            m_verticalThumb = std::make_unique<Frame>();
            m_verticalThumb->setBaseProperties({.zIndex = getZIndex() + 2});
            m_verticalThumb->name = "vertical thumb";
            m_verticalThumb->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }

        float trackHeight = absoluteSize.y - (needsHorizontal ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = absoluteSize.y / absCanvasSize.y;
        float thumbHeight = trackHeight * thumbRatio;
        float scrollRatio = m_scrollOffset.y / (absCanvasSize.y - absoluteSize.y);
        float thumbY = scrollRatio * (trackHeight - thumbHeight);

        m_verticalBar->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarColor,
            .backgroundTransparency = m_sfProps.scrollBarTransparency,
        });
        m_verticalBar->clipRect = clipRect;
        m_verticalBar->markDirty();
        m_verticalBar->computeAbsolutes(glm::vec2(m_sfProps.scrollBarThickness, trackHeight),
                                        absolutePosition + glm::vec2(absoluteSize.x - m_sfProps.scrollBarThickness, 0.0f),
                                        absoluteRotation);
        m_verticalBar->draw(ctx);

        m_verticalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_verticalThumb->clipRect = clipRect;
        m_verticalThumb->markDirty();
        m_verticalThumb->computeAbsolutes(glm::vec2(m_sfProps.scrollBarThickness, thumbHeight),
                                          absolutePosition + glm::vec2(absoluteSize.x - m_sfProps.scrollBarThickness, thumbY),
                                          absoluteRotation);
        m_verticalThumb->draw(ctx);
    } else {
        m_verticalBar.reset();
        m_verticalThumb.reset();
    }

    if (needsHorizontal) {
        if (m_horizontalBar == nullptr) {
            m_horizontalBar = std::make_unique<Frame>();
            m_horizontalBar->name = "Horizontal Bar";
            m_horizontalBar->setBaseProperties({.zIndex = getZIndex() + 1});
            m_horizontalBar->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }
        if (m_horizontalThumb == nullptr) {
            m_horizontalThumb = std::make_unique<Frame>();
            m_horizontalThumb->setBaseProperties({.zIndex = getZIndex() + 2});
            m_horizontalThumb->name = "Horizontal Thumb";
            m_horizontalThumb->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }

        float trackWidth = absoluteSize.x - (needsVertical ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = absoluteSize.x / absCanvasSize.x;
        float thumbWidth = trackWidth * thumbRatio;
        float scrollRatio = m_scrollOffset.x / (absCanvasSize.x - absoluteSize.x);
        float thumbX = scrollRatio * (trackWidth - thumbWidth);

        m_horizontalBar->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarColor,
            .backgroundTransparency = m_sfProps.scrollBarTransparency,
        });
        m_horizontalBar->clipRect = clipRect;
        m_horizontalBar->markDirty();
        m_horizontalBar->computeAbsolutes(glm::vec2(trackWidth, m_sfProps.scrollBarThickness),
                                          absolutePosition + glm::vec2(0.0f, absoluteSize.y - m_sfProps.scrollBarThickness),
                                          absoluteRotation);
        m_horizontalBar->draw(ctx);

        m_horizontalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_horizontalThumb->clipRect = clipRect;
        m_horizontalThumb->markDirty();
        m_horizontalThumb->computeAbsolutes(glm::vec2(thumbWidth, m_sfProps.scrollBarThickness),
                                            absolutePosition + glm::vec2(thumbX, absoluteSize.y - m_sfProps.scrollBarThickness),
                                            absoluteRotation);
        m_horizontalThumb->draw(ctx);
    } else {
        m_horizontalBar.reset();
        m_horizontalThumb.reset();
    }
}

EventResult ScrollingFrame::onMouseScrollUp()
{
    if (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) {
        m_scrollOffset.y -= m_sfProps.scrollSpeed;
    } else {
        m_scrollOffset.x -= m_sfProps.scrollSpeed;
    }
    markDirty();
    return EventResult::CONSUMED;
}

EventResult ScrollingFrame::onMouseScrollDown()
{
    if (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) {
        m_scrollOffset.y += m_sfProps.scrollSpeed;
    } else {
        m_scrollOffset.x += m_sfProps.scrollSpeed;
    }
    markDirty();
    return EventResult::CONSUMED;
}

} // namespace Amethyst
