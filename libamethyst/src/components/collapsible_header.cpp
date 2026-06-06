#include "components/collapsible_header.h"

#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/text_label.h"
#include "components/ui_layer.h"
// #include "modules/style.h"  // TODO: update style system to use property structs
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

// static void applyStyle(CollapsibleHeader &ch) { ... }  // TODO: update style system to use property structs

CollapsibleHeader::CollapsibleHeader() : CollapsibleHeader(nullptr, nullptr) {}

CollapsibleHeader::CollapsibleHeader(std::unique_ptr<UIObject> customIndicator, std::unique_ptr<UIObject> customHeader)
{
    m_chProps.expanded = true;
    m_chProps.titleStyle.fontSize = 14.0f;
    m_chProps.titleStyle.textColor = Color4{1.0f, 1.0f, 1.0f, 1.0f};
    m_chProps.titleStyle.textXAlignment = TextXAlignment::LEFT;
    m_chProps.titleStyle.textYAlignment = TextYAlignment::CENTER;
    m_chProps.headerHeight = 30.0f;
    m_chProps.headerTransparency = 0.0f;
    m_chProps.headerCornerRadius = 0.0f;
    m_chProps.showIndicator = true;
    m_chProps.indicatorSize = 10.0f;
    m_chProps.indicatorPadding = 6.0f;
    m_chProps.indicatorColor = Color4{0.7f, 0.7f, 0.7f, 1.0f};

    // applyStyle(*this);

    m_headerBackground = std::make_unique<Frame>();
    m_headerBackground->parent = this;
    m_headerBackground->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    m_headerBackground->addClass("collapsible-header#header");

    m_headerButton = std::make_unique<InvisibleButton>();
    m_headerButton->parent = this;
    m_headerButton->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    m_headerButton->onMouseButton1ClickCb = [this]() {
        toggle();
        return EventResult::CONSUMED;
    };

    if (customIndicator != nullptr) {
        m_indicator = customIndicator.get();
        m_headerBackground->addChild(std::move(customIndicator));
    } else {
        auto indicator = std::make_unique<Frame>();
        m_indicator = indicator.get();
        m_headerBackground->addChild(std::move(indicator));
    }

    if (customHeader != nullptr) {
        m_headerContent = customHeader.get();
        m_headerBackground->addChild(std::move(customHeader));
    } else {
        auto label = std::make_unique<TextLabel>();
        m_headerContent = label.get();
        m_headerBackground->addChild(std::move(label));
    }
}

CollapsibleHeader::~CollapsibleHeader()
{
    m_headerBackground->parent = nullptr;
    m_headerButton->parent = nullptr;
}

void CollapsibleHeader::toggle()
{
    m_chProps.expanded = !static_cast<bool>(m_chProps.expanded);
    markDirty();
    if (onToggled) {
        onToggled(static_cast<bool>(m_chProps.expanded));
    }
}

void CollapsibleHeader::expand()
{
    if (!static_cast<bool>(m_chProps.expanded)) {
        m_chProps.expanded = true;
        markDirty();
        if (onToggled) {
            onToggled(true);
        }
    }
}

void CollapsibleHeader::collapse()
{
    if (static_cast<bool>(m_chProps.expanded)) {
        m_chProps.expanded = false;
        markDirty();
        if (onToggled) {
            onToggled(false);
        }
    }
}

bool CollapsibleHeader::setCollapsibleHeaderProperties(const CollapsibleHeaderStyleProperties &props)
{
    bool changed = m_chProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void CollapsibleHeader::setTitle(std::string title)
{
    if (m_title != title) {
        m_title = std::move(title);
        markDirty();
    }
}

CollapsibleHeader &CollapsibleHeader::header(std::function<void(Frame &)> fn)
{
    if (fn) {
        fn(*m_headerBackground);
    }
    return *this;
}

CollapsibleHeader &CollapsibleHeader::indicator(std::function<void(UIObject &)> fn)
{
    if (fn) {
        fn(*m_indicator);
    }
    return *this;
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
    bool expanded = static_cast<bool>(m_chProps.expanded);
    bool showIndicator = static_cast<bool>(m_chProps.showIndicator);
    float contentOffset =
        m_chProps.indicatorPadding + (showIndicator ? m_chProps.indicatorSize + m_chProps.indicatorPadding : 0.0f);

    m_indicator->setBaseStyleProperties({
        .backgroundColor = Color3(m_chProps.indicatorColor),
        .backgroundTransparency = 1.0f - m_chProps.indicatorColor.a,
        .borderPixelSize = 0.0f,
    });
    m_indicator->setBaseProperties({
        .anchorPoint = {0.5f, 0.5f},
        .position = UDim2{{0.0f, m_chProps.indicatorPadding + m_chProps.indicatorSize * 0.5f}, {0.5f, 0.0f}},
        .size = UDim2::fromOffset(m_chProps.indicatorSize, m_chProps.indicatorSize),
        .rotation = expanded ? 90.0f : 0.0f,
        .visible = m_chProps.showIndicator,
        .zIndex = getZIndex() + 2,
    });

    m_headerContent->setBaseProperties({
        .position = UDim2{{0.0f, contentOffset}, {0.0f, 0.0f}},
        .size = UDim2{{1.0f, -(contentOffset + m_chProps.indicatorPadding)}, {1.0f, 0.0f}},
        .visible = true,
        .zIndex = getZIndex() + 2,
    });

    if (auto *label = m_headerContent->as<TextLabel>()) {
        label->setTextStyleProperties({
            .fontSize = m_chProps.titleStyle.fontSize,
            .textColor = m_chProps.titleStyle.textColor,
            .textXAlignment = m_chProps.titleStyle.textXAlignment,
            .textYAlignment = m_chProps.titleStyle.textYAlignment,
            .fontFamily = m_chProps.titleStyle.fontFamily,
        });
        label->setText(m_title);
        label->setBaseStyleProperties({.backgroundTransparency = 1.0f});
    }

    m_headerBackground->setBaseStyleProperties({
        .backgroundColor = m_chProps.headerColor,
        .backgroundTransparency = m_chProps.headerTransparency,
        .borderPixelSize = 0.0f,
        .cornerRadius = m_chProps.headerCornerRadius,
    });
    m_headerBackground->setBaseProperties({.zIndex = getZIndex() + 1});
    m_headerBackground->clipRect = childClip;
    m_headerBackground->markDirty();
    m_headerBackground->computeAbsolutes({absoluteSize.x, m_chProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->draw(ctx);

    m_headerButton->setBaseProperties({.zIndex = getZIndex() + 3});
    m_headerButton->clipRect = childClip;
    m_headerButton->markDirty();
    m_headerButton->computeAbsolutes({absoluteSize.x, m_chProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerButton->draw(ctx);

    if (expanded) {
        if (auto *gridLayout = getExtension<UIGridLayout>()) {
            gridLayout->apply(m_children);
        } else if (auto *listLayout = getExtension<UIListLayout>()) {
            listLayout->apply(m_children);
        }

        glm::vec2 contentPos = absoluteContentPosition + glm::vec2(0.0f, m_chProps.headerHeight);
        glm::vec2 contentSize = {absoluteContentSize.x, glm::max(absoluteContentSize.y - m_chProps.headerHeight, 0.0f)};

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
                int8_t originalVisible = drawable->getBaseProperties().visible;
                drawable->setBaseProperties({.visible = false});
                drawable->markDirty();
                drawable->clipRect = childClip;
                drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
                drawable->draw(ctx);
                drawable->setBaseProperties({.visible = originalVisible});
            }
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

std::vector<Instance *> CollapsibleHeader::getHittableInstances()
{
    std::vector<Instance *> result;
    result.push_back(m_headerButton.get());
    result.push_back(m_headerContent);

    if (static_cast<bool>(m_chProps.expanded)) {
        for (auto &child : m_children) {
            result.push_back(child.get());
        }
    }

    return result;
}

} // namespace Amethyst
