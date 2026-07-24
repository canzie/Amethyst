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
    m_sfProps.elasticScrolling = false;

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
    resolveBaseStyle(ComponentType::SCROLLING_FRAME);

    ScrollingFrameStyleProperties resolved =
        Style::instance().getScrollingFrameStyle(ComponentType::SCROLLING_FRAME, getClasses(), effectiveGuiState());
    if (m_sfProps.apply(resolved)) {
        markDirty();
    }
}

bool ScrollingFrame::setScrollingFrameProperties(const ScrollingFrameStylePropertiesArgs &props)
{
    bool changed = m_sfProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

vec2 ScrollingFrame::getScrollFraction() const
{
    return {
        m_maxScroll.x > 0.0f ? m_scrollOffset.x / m_maxScroll.x : 0.0f,
        m_maxScroll.y > 0.0f ? m_scrollOffset.y / m_maxScroll.y : 0.0f,
    };
}

void ScrollingFrame::layoutChildren(vec2 &absCanvasSize, vec2 viewport)
{
    AutomaticSize acs = m_sfProps.automaticCanvasSize;
    bool autoCanvas = acs != AutomaticSize::NONE && acs != AutomaticSize::OFF;

    vec2 origin = absoluteContentPosition - m_scrollOffset;
    vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        auto *obj = child->asUiObject();
        if (obj == nullptr) {
            continue;
        }

        obj->clipRect = childClip;
        obj->computeAbsolutes(absCanvasSize, origin, absoluteRotation);
        obj->arrange();

        if (autoCanvas) {
            vec2 childCanvasOffset = obj->absolutePosition - origin;
            if (acs == AutomaticSize::X || acs == AutomaticSize::XY) {
                absCanvasSize.x = std::max(absCanvasSize.x, childCanvasOffset.x + obj->absoluteSize.x);
            }
            if (acs == AutomaticSize::Y || acs == AutomaticSize::XY) {
                absCanvasSize.y = std::max(absCanvasSize.y, childCanvasOffset.y + obj->absoluteSize.y);
            }
        }

        vec2 childEffectivePos = obj->absolutePosition - absoluteContentPosition;
        bool inViewport = (childEffectivePos.x + obj->absoluteSize.x > 0.0f) && (childEffectivePos.x < viewport.x) &&
                          (childEffectivePos.y + obj->absoluteSize.y > 0.0f) && (childEffectivePos.y < viewport.y);
        obj->setRenderCulled(!inViewport);
    }
}

void ScrollingFrame::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    applyLayoutExtensions();

    m_scrollOffset = clamp(m_scrollOffset, vec2(0.0f), m_maxScroll);

    vec2 viewport = absoluteContentSize;
    vec2 absCanvasSize = m_sfProps.canvasSize.resolve(viewport);
    layoutChildren(absCanvasSize, viewport);
    m_maxScroll = max(absCanvasSize - viewport, vec2(0.0f));

    bool barsEnabled = m_sfProps.scrollBarVisibility != ScrollBarVisibility::NEVER && isVisible();
    m_needsVertical =
        barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::Y || m_sfProps.scrollAxis == ScrollAxis::XY) && m_maxScroll.y > 0.0f;
    m_needsHorizontal =
        barsEnabled && (m_sfProps.scrollAxis == ScrollAxis::X || m_sfProps.scrollAxis == ScrollAxis::XY) && m_maxScroll.x > 0.0f;

    if (m_needsVertical || m_needsHorizontal) {
        if (m_needsVertical) {
            viewport.x -= m_sfProps.scrollBarThickness;
        }
        if (m_needsHorizontal) {
            viewport.y -= m_sfProps.scrollBarThickness;
        }
        absoluteContentSize = viewport;
        absCanvasSize = m_sfProps.canvasSize.resolve(viewport);
        layoutChildren(absCanvasSize, viewport);
        m_maxScroll = max(absCanvasSize - viewport, vec2(0.0f));
        m_scrollOffset = clamp(m_scrollOffset, vec2(0.0f), m_maxScroll);
    }

    m_absCanvasSize = absCanvasSize;
    arrangeScrollbars();
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

    drawChildren(ctx);
    drawScrollbars(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void ScrollingFrame::arrangeScrollbars()
{
    vec2 viewport = absoluteContentSize;

    if (m_needsVertical) {
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

        float trackHeight = absoluteSize.y - (m_needsHorizontal ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = viewport.y / m_absCanvasSize.y;
        float thumbHeight = trackHeight * thumbRatio;
        float scrollRatio = m_scrollOffset.y / (m_absCanvasSize.y - viewport.y);
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
        m_verticalBar->arrange();

        m_verticalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_verticalThumb->clipRect = clipRect;
        m_verticalThumb->markDirty();
        m_verticalThumb->computeAbsolutes(vec2(m_sfProps.scrollBarThickness, thumbHeight),
                                          absolutePosition + vec2(absoluteSize.x - m_sfProps.scrollBarThickness, thumbY),
                                          absoluteRotation);
        m_verticalThumb->arrange();
    } else {
        m_verticalBar.reset();
        m_verticalThumb.reset();
    }

    if (m_needsHorizontal) {
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

        float trackWidth = absoluteSize.x - (m_needsVertical ? m_sfProps.scrollBarThickness : 0.0f);
        float thumbRatio = viewport.x / m_absCanvasSize.x;
        float thumbWidth = trackWidth * thumbRatio;
        float scrollRatio = m_scrollOffset.x / (m_absCanvasSize.x - viewport.x);
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
        m_horizontalBar->arrange();

        m_horizontalThumb->setBaseStyleProperties({
            .backgroundColor = m_sfProps.scrollBarThumbColor,
            .backgroundTransparency = m_sfProps.scrollBarThumbTransparency,
        });
        m_horizontalThumb->clipRect = clipRect;
        m_horizontalThumb->markDirty();
        m_horizontalThumb->computeAbsolutes(vec2(thumbWidth, m_sfProps.scrollBarThickness),
                                            absolutePosition + vec2(thumbX, absoluteSize.y - m_sfProps.scrollBarThickness),
                                            absoluteRotation);
        m_horizontalThumb->arrange();
    } else {
        m_horizontalBar.reset();
        m_horizontalThumb.reset();
    }
}

void ScrollingFrame::drawScrollbars(DrawContext &ctx)
{
    if (m_verticalBar != nullptr) {
        m_verticalBar->draw(ctx);
    }
    if (m_verticalThumb != nullptr) {
        m_verticalThumb->draw(ctx);
    }
    if (m_horizontalBar != nullptr) {
        m_horizontalBar->draw(ctx);
    }
    if (m_horizontalThumb != nullptr) {
        m_horizontalThumb->draw(ctx);
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
