#include "components/collapsible_header.h"

#include "amethyst/icons.h"
#include "components/extensions/ui_grid_layout.h"
#include "components/extensions/ui_list_layout.h"
#include "components/image_label.h"
#include "components/text_label.h"
#include "components/ui_layer.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "utils/profiling.h"

namespace Amethyst {

// level with the header background and listed before it, so whatever sits on the header is hit first and only
// clicks that pass through it reach the toggle button
static constexpr int32_t Z_HEADER_BUTTON = 1;

CollapsibleHeader::CollapsibleHeader() : CollapsibleHeader(nullptr, nullptr) {}

CollapsibleHeader::CollapsibleHeader(std::unique_ptr<UIObject> customIndicator, std::unique_ptr<UIObject> customHeader)
{
    m_chProps.expanded = true;
    m_chProps.titleStyle.fontSize = 14.0f;
    m_chProps.titleStyle.textColor = Color4{1.0f, 1.0f, 1.0f, 1.0f};
    m_chProps.titleStyle.textXAlignment = TextXAlignment::LEFT;
    m_chProps.titleStyle.textYAlignment = TextYAlignment::CENTER;
    m_chProps.headerHeight = 30.0f;
    m_chProps.showIndicator = true;
    m_chProps.indicatorSize = 16.0f;
    m_chProps.indicatorPadding = 6.0f;
    m_chProps.indicatorColor = Color4{0.7f, 0.7f, 0.7f, 1.0f};

    resolveStyle();

    m_headerBackground = std::make_unique<Frame>();
    m_headerBackground->parent = this;
    m_headerBackground->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    m_headerBackground->bindPart(ComponentPart::HEADER);
    // the background lies over the toggle button as a surface for header content, so it hears input
    // without taking it from the button; controls placed on it still take their own clicks
    m_headerBackground->propagate(INTERACTION_CATEGORY_ALL);
    // inherited, not assigned, as TabBar does: classes set on the header pieces survive, and a class
    // removed from the header leaves them too
    m_headerBackground->setInheritedStyle(getEffectiveClasses());

    m_headerButton = std::make_unique<InvisibleButton>();
    m_headerButton->parent = this;
    m_headerButton->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f)});
    m_headerButton->track(m_headerButton->onMouseButton1ClickCb.connect([this]() { toggle(); }));

    if (customIndicator != nullptr) {
        m_indicator = customIndicator.get();
        m_headerBackground->addChild(std::move(customIndicator));
    } else {
        auto indicator = std::make_unique<ImageLabel>();
        m_indicator = indicator.get();
        indicator->setSvg(Icons::ARROW);
        m_headerBackground->addChild(std::move(indicator));
    }
    m_indicator->bindPart(ComponentPart::INDICATOR);
    m_indicator->setInheritedStyle(getEffectiveClasses());
    m_indicator->propagateClassesToChildren();

    if (customHeader != nullptr) {
        m_headerContent = customHeader.get();
        m_headerBackground->addChild(std::move(customHeader));
    } else {
        auto label = std::make_unique<TextLabel>();
        m_headerContent = label.get();
        m_headerBackground->addChild(std::move(label));
    }
    m_headerContent->bindPart(ComponentPart::HEADER);
    m_headerContent->setInheritedStyle(getEffectiveClasses());
    m_headerContent->propagateClassesToChildren();

}

CollapsibleHeader::~CollapsibleHeader()
{
    m_headerBackground->parent = nullptr;
    m_headerButton->parent = nullptr;
}

void CollapsibleHeader::resolveStyle()
{
    resolveBaseStyle(ComponentType::COLLAPSIBLE_HEADER);

    CollapsibleHeaderStyleProperties resolved =
        Style::instance().getCollapsibleHeaderStyle(ComponentType::COLLAPSIBLE_HEADER, getClasses(), effectiveGuiState());
    if (m_chProps.apply(resolved)) {
        markDirty();
    }

    std::vector<StyleKey> classes = getEffectiveClasses();
    if (m_headerBackground != nullptr) {
        m_headerBackground->setInheritedStyle(classes);
    }
    if (m_indicator != nullptr) {
        m_indicator->setInheritedStyle(classes);
        m_indicator->propagateClassesToChildren();
    }
    if (m_headerContent != nullptr) {
        m_headerContent->setInheritedStyle(classes);
        m_headerContent->propagateClassesToChildren();
    }
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

bool CollapsibleHeader::setCollapsibleHeaderProperties(const CollapsibleHeaderStylePropertiesArgs &props)
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
        m_headerBackground->propagateClassesToChildren();
        m_indicator->bindPart(ComponentPart::INDICATOR);
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

void CollapsibleHeader::computeAbsolutes(vec2 parentSize, vec2 parentPos, Degrees parentRotation)
{
    UIObject::computeAbsolutes(parentSize, parentPos, parentRotation);
    if (!static_cast<bool>(m_chProps.expanded)) {
        absoluteSize.y = m_chProps.headerHeight;
    }
}

void CollapsibleHeader::arrangeHeaderBar()
{
    bool expanded = static_cast<bool>(m_chProps.expanded);
    bool showIndicator = static_cast<bool>(m_chProps.showIndicator);
    float contentOffset =
        m_chProps.indicatorPadding + (showIndicator ? m_chProps.indicatorSize + m_chProps.indicatorPadding : 0.0f);

    m_indicator->setBaseStyleProperties({
        .backgroundTransparency = 1.0f,
        .borderPixelSize = 0.0f,
    });
    if (auto *imgLabel = m_indicator->as<ImageLabel>()) {
        imgLabel->setImageStyleProperties({
            .imageColor = m_chProps.indicatorColor,
        });
    }
    m_indicator->setBaseProperties({
        .anchorPoint = {0.5f, 0.5f},
        .position = UDim2(0.0f, m_chProps.indicatorPadding + m_chProps.indicatorSize * 0.5f, 0.5f, 0.0f),
        .size = UDim2::fromOffset(m_chProps.indicatorSize, m_chProps.indicatorSize),
        .rotation = expanded ? 90.0f : 0.0f,
        .visible = m_chProps.showIndicator,
    });

    m_headerContent->setBaseProperties({
        .position = UDim2(0.0f, contentOffset, 0.0f, 0.0f),
        .size = UDim2(1.0f, -(contentOffset + m_chProps.indicatorPadding), 1.0f, 0.0f),
        .visible = true,
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

    vec4 childClip = computeChildClipRect();
    m_headerBackground->clipRect = childClip;
    m_headerBackground->computeAbsolutes({absoluteSize.x, m_chProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerBackground->arrange();

    m_headerButton->setBaseProperties({.zIndex = Z_HEADER_BUTTON});
    m_headerButton->clipRect = childClip;
    m_headerButton->computeAbsolutes({absoluteSize.x, m_chProps.headerHeight}, absolutePosition, absoluteRotation);
    m_headerButton->arrange();
}

void CollapsibleHeader::arrange()
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }
    refreshStyleIfStale();

    arrangeHeaderBar();

    bool expanded = static_cast<bool>(m_chProps.expanded);
    if (expanded) {
        applyLayoutExtensions();
    }

    vec2 contentPos = absoluteContentPosition + vec2(0.0f, m_chProps.headerHeight);
    vec2 contentSize = {absoluteContentSize.x, max(absoluteContentSize.y - m_chProps.headerHeight, 0.0f)};

    vec4 childClip = computeChildClipRect();
    for (auto &child : m_children) {
        if (auto *obj = child->asUiObject()) {
            obj->setRenderCulled(!expanded);
            obj->clipRect = childClip;
            obj->computeAbsolutes(contentSize, contentPos, absoluteRotation);
            obj->arrange();
        } else if (auto *layer = child->asLayer()) {
            layer->arrange();
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

        pushData(ctx.geometry, data);
    }

    m_headerBackground->draw(ctx);
    m_headerButton->draw(ctx);

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

void CollapsibleHeader::markChildrenDirty()
{
    Instance::markChildrenDirty();

    m_headerBackground->markChildrenDirty();
    m_headerBackground->flags |= FLAG_DIRTY;
    m_headerButton->flags |= FLAG_DIRTY;
}

std::vector<Instance *> CollapsibleHeader::getHittableInstances()
{
    std::vector<Instance *> result;
    result.push_back(m_headerButton.get());
    // the whole header, so controls placed on it through header() can be hit
    result.push_back(m_headerBackground.get());

    if (static_cast<bool>(m_chProps.expanded)) {
        for (auto &child : m_children) {
            result.push_back(child.get());
        }
    }

    return result;
}

} // namespace Amethyst
