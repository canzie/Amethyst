#include "components/collapsible_header.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/ui_layer.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

static void applyStyle(CollapsibleHeader &ch)
{
    const auto &style = Style::instance();
    ch.backgroundColor = style.get<Color3>(StyleProperty::BACKGROUND_COLOR, ComponentType::COLLAPSIBLE_HEADER);
    ch.backgroundTransparency =
        style.get<float>(StyleProperty::BACKGROUND_TRANSPARENCY, ComponentType::COLLAPSIBLE_HEADER);
    ch.borderColor = style.get<Color3>(StyleProperty::BORDER_COLOR, ComponentType::COLLAPSIBLE_HEADER);
    ch.borderTransparency = style.get<float>(StyleProperty::BORDER_TRANSPARENCY, ComponentType::COLLAPSIBLE_HEADER);
    ch.borderPixelSize = style.get<float>(StyleProperty::BORDER_PIXEL_SIZE, ComponentType::COLLAPSIBLE_HEADER);
    ch.cornerRadius = style.get<float>(StyleProperty::CORNER_RADIUS, ComponentType::COLLAPSIBLE_HEADER);
    ch.headerColor = style.get<Color3>(StyleProperty::HEADER_COLOR, ComponentType::COLLAPSIBLE_HEADER);
    ch.headerTransparency = style.get<float>(StyleProperty::HEADER_TRANSPARENCY, ComponentType::COLLAPSIBLE_HEADER);
    ch.headerHeight = style.get<float>(StyleProperty::HEADER_HEIGHT, ComponentType::COLLAPSIBLE_HEADER);
    ch.titleColor = style.get<Color4>(StyleProperty::TEXT_COLOR, ComponentType::COLLAPSIBLE_HEADER);
    ch.fontSize = style.get<float>(StyleProperty::FONT_SIZE, ComponentType::COLLAPSIBLE_HEADER);
    ch.indicatorSize =
        style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_SIZE, ComponentType::COLLAPSIBLE_HEADER);
    ch.indicatorPadding =
        style.get<float>(StyleProperty::DISCLOSURE_TRIANGLE_PADDING, ComponentType::COLLAPSIBLE_HEADER);
    ch.indicatorColor =
        style.get<Color4>(StyleProperty::DISCLOSURE_TRIANGLE_COLOR, ComponentType::COLLAPSIBLE_HEADER);
}

CollapsibleHeader::CollapsibleHeader()
{
    applyStyle(*this);

    m_headerBackground = std::make_unique<Frame>();
    m_headerBackground->parent = this;
    m_headerBackground->interactable = false;
    m_headerBackground->size = UDim2::fromScale(1.0f, 1.0f);

    m_headerButton = std::make_unique<InvisibleButton>();
    m_headerButton->parent = this;
    m_headerButton->size = UDim2::fromScale(1.0f, 1.0f);
    m_headerButton->onMouseButton1ClickCb = [this]() {
        toggle();
        return EventResult::CONSUMED;
    };

    m_indicator = std::make_unique<Frame>();
    m_indicator->parent = this;
    m_indicator->interactable = false;
    m_indicator->size = UDim2::fromScale(1.0f, 1.0f);

    m_titleLabel = std::make_unique<TextLabel>();
    m_titleLabel->parent = this;
    m_titleLabel->interactable = false;
    m_titleLabel->size = UDim2::fromScale(1.0f, 1.0f);
}

CollapsibleHeader::~CollapsibleHeader()
{
    m_headerBackground->parent = nullptr;
    m_headerButton->parent = nullptr;
    m_indicator->parent = nullptr;
    m_titleLabel->parent = nullptr;
}

void CollapsibleHeader::toggle()
{
    expanded = !expanded;
    markDirty();
    if (onToggled) {
        onToggled(expanded);
    }
}

void CollapsibleHeader::expand()
{
    if (!expanded) {
        expanded = true;
        markDirty();
        if (onToggled) {
            onToggled(true);
        }
    }
}

void CollapsibleHeader::collapse()
{
    if (expanded) {
        expanded = false;
        markDirty();
        if (onToggled) {
            onToggled(false);
        }
    }
}

void CollapsibleHeader::draw(DrawContext &ctx)
{
    AM_PROFILE_FUNCTION();
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

    glm::vec4 childClip = computeChildClipRect();

    m_headerBackground->backgroundColor = headerColor;
    m_headerBackground->backgroundTransparency = headerTransparency;
    m_headerBackground->cornerRadius = headerCornerRadius;
    m_headerBackground->borderPixelSize = 0.0f;
    m_headerBackground->zIndex = getZIndex() + 1;
    m_headerBackground->clipRect = childClip;
    m_headerBackground->markDirty();
    m_headerBackground->computeAbsolutes({absoluteSize.x, headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->draw(ctx);

    if (showIndicator) {
        float indicatorX = absolutePosition.x + indicatorPadding;
        float indicatorCenterY = absolutePosition.y + headerHeight * 0.5f;

        m_indicator->visible = true;
        m_indicator->backgroundColor = Color3(indicatorColor);
        m_indicator->backgroundTransparency = 1.0f - indicatorColor.a;
        m_indicator->borderPixelSize = 0.0f;
        m_indicator->rotation = expanded ? 90.0f : 0.0f;
        m_indicator->anchorPoint = {0.5f, 0.5f};
        m_indicator->zIndex = getZIndex() + 2;
        m_indicator->clipRect = childClip;
        m_indicator->markDirty();
        m_indicator->computeAbsolutes({indicatorSize, indicatorSize},
                                      {indicatorX + indicatorSize * 0.5f, indicatorCenterY}, absoluteRotation);
        m_indicator->draw(ctx);
    } else {
        m_indicator->visible = false;
        m_indicator->markDirty();
        m_indicator->draw(ctx);
    }

    float titleOffset = indicatorPadding;
    if (showIndicator) {
        titleOffset += indicatorSize + indicatorPadding;
    }
    float titleWidth = absoluteSize.x - titleOffset - indicatorPadding;

    m_titleLabel->text = title;
    m_titleLabel->fontFamily = fontFamily;
    m_titleLabel->fontSize = fontSize;
    m_titleLabel->textColor = titleColor;
    m_titleLabel->textXAlignment = titleXAlignment;
    m_titleLabel->textYAlignment = titleYAlignment;
    m_titleLabel->backgroundTransparency = 1.0f;
    m_titleLabel->zIndex = getZIndex() + 2;
    m_titleLabel->clipRect = childClip;
    m_titleLabel->visible = true;
    m_titleLabel->markDirty();
    m_titleLabel->computeAbsolutes({titleWidth, headerHeight},
                                   absolutePosition + glm::vec2(titleOffset, 0.0f), absoluteRotation);
    m_titleLabel->draw(ctx);

    m_headerButton->zIndex = getZIndex() + 3;
    m_headerButton->clipRect = childClip;
    m_headerButton->markDirty();
    m_headerButton->computeAbsolutes({absoluteSize.x, headerHeight}, absolutePosition, absoluteRotation);
    m_headerButton->draw(ctx);

    if (expanded) {
        if (auto *gridLayout = getExtension<UIGridLayout>()) {
            gridLayout->apply(m_children);
        } else if (auto *listLayout = getExtension<UIListLayout>()) {
            listLayout->apply(m_children);
        }

        glm::vec2 contentPos = absoluteContentPosition + glm::vec2(0.0f, headerHeight);
        glm::vec2 contentSize = {absoluteContentSize.x,
                                 glm::max(absoluteContentSize.y - headerHeight, 0.0f)};

        for (auto &child : m_children) {
            if (auto *drawable = child->as<UIObject>()) {
                drawable->clipRect = childClip;
                drawable->computeAbsolutes(contentSize, contentPos, absoluteRotation);
                drawable->draw(ctx);
            } else if (auto *layer = child->as<UILayer>()) {
                layer->draw(ctx);
            }
        }
    } else {
        for (auto &child : m_children) {
            if (auto *drawable = child->as<UIObject>()) {
                bool originalVisible = drawable->visible;
                drawable->visible = false;
                drawable->markDirty();
                drawable->clipRect = childClip;
                drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
                drawable->draw(ctx);
                drawable->visible = originalVisible;
            }
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> CollapsibleHeader::getHittableInstances()
{
    std::vector<Instance *> result;
    result.push_back(m_headerButton.get());

    if (expanded) {
        for (auto &child : m_children) {
            result.push_back(child.get());
        }
    }

    return result;
}

} // namespace Amethyst
