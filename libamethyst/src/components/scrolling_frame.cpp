#include "components/scrolling_frame.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

static constexpr int32_t Z_SCROLLBAR = 100;
static constexpr int32_t Z_SCROLLBAR_THUMB = 101;

ScrollingFrame::ScrollingFrame()
{
    consume(INTERACTION_CATEGORY_SCROLL);
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

ScrollingFrame::~ScrollingFrame()
{
    if (m_verticalBar != nullptr) {
        m_verticalBar->parent = nullptr;
    }
    if (m_verticalThumb != nullptr) {
        m_verticalThumb->parent = nullptr;
    }
    if (m_horizontalBar != nullptr) {
        m_horizontalBar->parent = nullptr;
    }
    if (m_horizontalThumb != nullptr) {
        m_horizontalThumb->parent = nullptr;
    }
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

void ScrollingFrame::computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation)
{
    UIObject::computeAbsolutes(parentSize, parentPos, parentRotation);

    bool barsEnabled = m_sfProps.scrollBarVisibility != ScrollBarVisibility::NEVER && isVisible();
    bool needsVertical = barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                         m_maxScroll.y > 0.0f;
    bool needsHorizontal = barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::X || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                           m_maxScroll.x > 0.0f;

    if (needsVertical) {
        absoluteContentSize.x -= m_sfProps.scrollBarThickness;
    }
    if (needsHorizontal) {
        absoluteContentSize.y -= m_sfProps.scrollBarThickness;
    }
}

void ScrollingFrame::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);

        pushData(ctx.geometry, data);
    }

    AutomaticSize acs = m_sfProps.automaticCanvasSize;
    bool autoCanvas = acs != AutomaticSize::NONE && acs != AutomaticSize::OFF;

    bool barsEnabled = m_sfProps.scrollBarVisibility != ScrollBarVisibility::NEVER && isVisible();
    bool needsVertical = barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                         m_maxScroll.y > 0.0f;
    bool needsHorizontal = barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::X || m_sfProps.scrollAxis == ScrollAxis::XY) &&
                           m_maxScroll.x > 0.0f;

    vec2 absCanvasSize = m_sfProps.canvasSize.resolve(absoluteContentSize);

    m_scrollOffset = clamp(m_scrollOffset, vec2(0.0f), m_maxScroll);

    if (auto *gridLayout = getExtension<UIGridLayout>()) {
        gridLayout->apply(m_children);
    } else if (auto *listLayout = getExtension<UIListLayout>()) {
        listLayout->apply(m_children);
    }

    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        auto *obj = child->as<UIObject>();
        if (obj == nullptr) continue;

        obj->clipRect = childClip;
        obj->computeAbsolutes(absCanvasSize, absoluteContentPosition - m_scrollOffset, absoluteRotation);

        if (autoCanvas) {
            vec2 childCanvasOffset = obj->absolutePosition - (absoluteContentPosition - m_scrollOffset);
            if (acs == AutomaticSize::X || acs == AutomaticSize::XY)
                absCanvasSize.x = std::max(absCanvasSize.x, childCanvasOffset.x + obj->absoluteSize.x);
            if (acs == AutomaticSize::Y || acs == AutomaticSize::XY)
                absCanvasSize.y = std::max(absCanvasSize.y, childCanvasOffset.y + obj->absoluteSize.y);
        }

        vec2 childEffectivePos = obj->absolutePosition - absoluteContentPosition;
        bool inViewport = (childEffectivePos.x + obj->absoluteSize.x > 0.0f) && (childEffectivePos.x < absoluteContentSize.x) &&
                          (childEffectivePos.y + obj->absoluteSize.y > 0.0f) && (childEffectivePos.y < absoluteContentSize.y);

        bool localVisible = obj->getBaseProperties().visible != 0;
        bool effective = obj->isVisible() && inViewport;

        auto &lastViewport = m_childViewportVisibility[child.get()];
        if (effective != lastViewport) {
            obj->markDirty();
            lastViewport = effective;
        }

        obj->setBaseProperties({.visible = static_cast<am_bool>(effective)});
        obj->draw(ctx);
        obj->setBaseProperties({.visible = static_cast<am_bool>(localVisible)});
    }

    m_maxScroll = max(absCanvasSize - absoluteContentSize, vec2(0.0f));
    drawScrollbars(ctx, absCanvasSize, absoluteContentSize, needsVertical, needsHorizontal);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void ScrollingFrame::drawScrollbars(DrawContext &ctx, vec2 absCanvasSize, vec2 viewport, bool needsVertical,
                                    bool needsHorizontal)
{
    if (needsVertical) {
        if (m_verticalBar == nullptr) {
            m_verticalBar = std::make_unique<Frame>();
            m_verticalBar->parent = this;
            m_verticalBar->name = "vertical bar";
            m_verticalBar->setBaseProperties({.zIndex = Z_SCROLLBAR});
            m_verticalBar->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }
        if (m_verticalThumb == nullptr) {
            m_verticalThumb = std::make_unique<Frame>();
            m_verticalThumb->parent = this;
            m_verticalThumb->setBaseProperties({.zIndex = Z_SCROLLBAR_THUMB});
            m_verticalThumb->name = "vertical thumb";
            m_verticalThumb->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }

        float trackHeight = absoluteSize.y - (needsHorizontal ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = viewport.y / absCanvasSize.y;
        float thumbHeight = trackHeight * thumbRatio;
        float scrollRatio = m_scrollOffset.y / (absCanvasSize.y - viewport.y);
        float thumbY = scrollRatio * (trackHeight - thumbHeight);

        m_verticalBar->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarColor,
            .backgroundTransparency = m_sfProps.scrollBarTransparency,
        });
        m_verticalBar->clipRect = clipRect;
        m_verticalBar->markDirty();
        m_verticalBar->computeAbsolutes(vec2(m_sfProps.scrollBarThickness, trackHeight),
                                        absolutePosition + vec2(absoluteSize.x - m_sfProps.scrollBarThickness, 0.0f),
                                        absoluteRotation);
        m_verticalBar->draw(ctx);

        m_verticalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_verticalThumb->clipRect = clipRect;
        m_verticalThumb->markDirty();
        m_verticalThumb->computeAbsolutes(vec2(m_sfProps.scrollBarThickness, thumbHeight),
                                          absolutePosition + vec2(absoluteSize.x - m_sfProps.scrollBarThickness, thumbY),
                                          absoluteRotation);
        m_verticalThumb->draw(ctx);
    } else {
        m_verticalBar.reset();
        m_verticalThumb.reset();
    }

    if (needsHorizontal) {
        if (m_horizontalBar == nullptr) {
            m_horizontalBar = std::make_unique<Frame>();
            m_horizontalBar->parent = this;
            m_horizontalBar->name = "Horizontal Bar";
            m_horizontalBar->setBaseProperties({.zIndex = Z_SCROLLBAR});
            m_horizontalBar->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }
        if (m_horizontalThumb == nullptr) {
            m_horizontalThumb = std::make_unique<Frame>();
            m_horizontalThumb->parent = this;
            m_horizontalThumb->setBaseProperties({.zIndex = Z_SCROLLBAR_THUMB});
            m_horizontalThumb->name = "Horizontal Thumb";
            m_horizontalThumb->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
        }

        float trackWidth = absoluteSize.x - (needsVertical ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = viewport.x / absCanvasSize.x;
        float thumbWidth = trackWidth * thumbRatio;
        float scrollRatio = m_scrollOffset.x / (absCanvasSize.x - viewport.x);
        float thumbX = scrollRatio * (trackWidth - thumbWidth);

        m_horizontalBar->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarColor,
            .backgroundTransparency = m_sfProps.scrollBarTransparency,
        });
        m_horizontalBar->clipRect = clipRect;
        m_horizontalBar->markDirty();
        m_horizontalBar->computeAbsolutes(vec2(trackWidth, m_sfProps.scrollBarThickness),
                                          absolutePosition + vec2(0.0f, absoluteSize.y - m_sfProps.scrollBarThickness),
                                          absoluteRotation);
        m_horizontalBar->draw(ctx);

        m_horizontalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_horizontalThumb->clipRect = clipRect;
        m_horizontalThumb->markDirty();
        m_horizontalThumb->computeAbsolutes(vec2(thumbWidth, m_sfProps.scrollBarThickness),
                                            absolutePosition + vec2(thumbX, absoluteSize.y - m_sfProps.scrollBarThickness),
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
        m_scrollOffset.y = std::max(0.0f, m_scrollOffset.y - m_sfProps.scrollSpeed);
    } else {
        m_scrollOffset.x = std::max(0.0f, m_scrollOffset.x - m_sfProps.scrollSpeed);
    }
    markDirty();
    return EventResult::CONSUMED;
}

EventResult ScrollingFrame::onMouseScrollDown()
{
    if (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) {
        m_scrollOffset.y = std::min(m_maxScroll.y, m_scrollOffset.y + m_sfProps.scrollSpeed);
    } else {
        m_scrollOffset.x = std::min(m_maxScroll.x, m_scrollOffset.x + m_sfProps.scrollSpeed);
    }
    markDirty();
    return EventResult::CONSUMED;
}

} // namespace Amethyst
