#include "components/context_menu.h"

#include "amethyst/icons.h"
#include "components/checkbox.h"
#include "components/context_menu_item.h"
#include "components/frame.h"
#include "components/image_label.h"
#include "components/overlay_layer.h"
#include "components/radio_button.h"
#include "components/scrolling_frame.h"
#include "components/text_label.h"
#include "components/window.h"
#include "modules/style.h"

#include <algorithm>
#include <climits>

namespace Amethyst {

#define POPUP_ZINDEX 10

static constexpr float ICON_INSET = 4.0f;
static constexpr float ROW_PADDING = 8.0f;
static constexpr float SEPARATOR_HEIGHT = 4.0f;

static ComponentPart s_partForKind(ContextMenu::Kind kind)
{
    switch (kind) {
    case ContextMenu::Kind::ACTION:
        return ComponentPart::ACTION;
    case ContextMenu::Kind::TOGGLE:
        return ComponentPart::TOGGLE;
    case ContextMenu::Kind::SEPARATOR:
        return ComponentPart::SEPARATOR;
    case ContextMenu::Kind::SUBMENU:
        return ComponentPart::SUBMENU;
    case ContextMenu::Kind::RADIO:
        return ComponentPart::RADIO;
    }
    return ComponentPart::NONE;
}

static std::string s_buildItemText(const std::string &label, const std::string &shortcutHint)
{
    std::string text;
    text.reserve(label.size() + shortcutHint.size() + 8);
    text.append(label);
    if (!shortcutHint.empty()) {
        text.append("    ");
        text.append(shortcutHint);
    }
    return text;
}

Frame *ContextMenu::ItemView::create(ContextMenu &owner)
{
    m_owner = &owner;
    m_row = owner.add<Frame>();
    m_row->setBaseStyleProperties({.backgroundColor = owner.getBaseStyleProperties().backgroundColor,
                                   .backgroundTransparency = 0.0f,
                                   .borderPixelSize = 0.0f});
    // a menu is modal, so the press stops here instead of reaching whatever the popup covers
    m_row->consume(INTERACTION_CATEGORY_CLICK);
    m_row->track(m_row->onHoverChanged.connect([this](bool hovered) {
        if (hovered) {
            m_owner->closeSubmenu();
            m_row->setBaseStyleProperties({.backgroundColor = m_owner->getContextMenuProperties().itemHoverBackground});
        } else {
            m_row->setBaseStyleProperties({.backgroundColor = m_owner->getBaseStyleProperties().backgroundColor});
        }
    }));
    return m_row;
}

float ContextMenu::ItemView::rowHeight(const ContextMenu &owner) const
{
    return owner.itemHeight;
}

Frame *ContextMenu::ActionItemView::create(ContextMenu &owner)
{
    Frame *row = ItemView::create(owner);
    row->track(row->onInputBeganCb.connect([this](const InputObject &io) {
        if (io.type == InputType::MOUSE_BUTTON_1 && m_boundItem != nullptr) {
            activate(*m_owner, m_boundItem->as<ActionItemData>());
        }
    }));
    return row;
}

Frame *ContextMenu::ToggleItemView::create(ContextMenu &owner)
{
    Frame *row = ItemView::create(owner);
    row->track(row->onInputBeganCb.connect([this, row](const InputObject &io) {
        if (io.type == InputType::MOUSE_BUTTON_1 && m_boundItem != nullptr) {
            toggle(m_boundItem->as<ToggleItemData>());
            row->markDirty();
        }
    }));
    return row;
}

Frame *ContextMenu::SubmenuItemView::create(ContextMenu &owner)
{
    Frame *row = ItemView::create(owner);
    row->track(row->onHoverChanged.connect([this](bool hovered) {
        if (hovered && m_boundItem != nullptr) {
            openSubmenu(*m_owner, m_boundItem->as<SubmenuItemData>(), m_row);
        }
    }));
    return row;
}

Frame *ContextMenu::RadioItemView::create(ContextMenu &owner)
{
    Frame *row = ItemView::create(owner);
    row->track(row->onInputBeganCb.connect([this](const InputObject &io) {
        if (io.type == InputType::MOUSE_BUTTON_1 && m_boundItem != nullptr) {
            select(m_boundItem->as<RadioItemData>());
        }
    }));
    return row;
}

class DefaultActionItemView : public ContextMenu::ActionItemView {
  public:
    Frame *create(ContextMenu &owner) override
    {
        Frame *row = ActionItemView::create(owner);
        m_label = row->add<TextLabel>();
        m_label->setTextStyleProperties(owner.getTextStyleProperties());
        m_label->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
        m_label->setBaseProperties({
            .interactable = false,
            .padding =
                UDim4{UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING), UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING)},
            .size = UDim2::fromScale(1.0f, 1.0f),
        });
        return row;
    }

    void bind(ContextMenu::ItemData &item) override
    {
        m_boundItem = &item;
        auto &action = item.as<ContextMenuAction>();
        if (action.content) {
            m_label->setBaseProperties({.visible = false});
            action.content(*m_row);
        } else {
            m_label->setBaseProperties({.visible = true});
            m_label->setText(s_buildItemText(action.label, action.shortcutHint));
        }
        m_row->setBaseProperties({.interactable = action.enabled});
    }

  private:
    TextLabel *m_label = nullptr;
};

class DefaultToggleItemView : public ContextMenu::ToggleItemView {
  public:
    Frame *create(ContextMenu &owner) override
    {
        Frame *row = ToggleItemView::create(owner);
        m_label = row->add<TextLabel>();
        m_label->setTextStyleProperties(owner.getTextStyleProperties());
        m_label->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
        m_label->setBaseProperties({
            .interactable = false,
            .padding =
                UDim4{UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING), UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING)},
            .size = UDim2::fromScale(1.0f, 1.0f),
        });

        m_checkbox = row->add<Checkbox>();
        m_checkbox->setBaseProperties({
            .anchorPoint = {1.0f, 0.5f},
            .interactable = false, // decorative only since the row handles the click
            .position = UDim2(1.0f, 0.0f, 0.5f, 0.0f),
            .zIndex = POPUP_ZINDEX,
        });
        return row;
    }

    void bind(ContextMenu::ItemData &item) override
    {
        m_boundItem = &item;
        auto &toggleItem = item.as<ContextMenuToggle>();
        m_label->setText(s_buildItemText(toggleItem.label, toggleItem.shortcutHint));
        m_row->setBaseProperties({.interactable = toggleItem.enabled});

        float iconSize = m_owner->itemHeight - ICON_INSET;
        m_checkbox->setBaseProperties({.size = UDim2::fromOffset(iconSize, iconSize)});
        m_checkbox->value = &toggleItem.value;
        m_checkbox->markDirty();
    }

  private:
    TextLabel *m_label = nullptr;
    Checkbox *m_checkbox = nullptr;
};

class DefaultSeparatorItemView : public ContextMenu::SeparatorItemView {
  public:
    Frame *create(ContextMenu &owner) override
    {
        Frame *row = SeparatorItemView::create(owner);
        row->setBaseStyleProperties({
            .backgroundColor = owner.getContextMenuProperties().separatorColor,
            .backgroundTransparency = 0.5f,
            .borderPixelSize = 0.0f,
        });
        row->setBaseProperties({.interactable = false});
        return row;
    }

    void bind(ContextMenu::ItemData &) override {}

    float rowHeight(const ContextMenu &) const override { return SEPARATOR_HEIGHT; }
};

class DefaultSubmenuItemView : public ContextMenu::SubmenuItemView {
  public:
    Frame *create(ContextMenu &owner) override
    {
        Frame *row = SubmenuItemView::create(owner);
        m_label = row->add<TextLabel>();
        m_label->setTextStyleProperties(owner.getTextStyleProperties());
        m_label->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
        m_label->setBaseProperties({
            .interactable = false,
            .padding =
                UDim4{UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING), UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING)},
            .size = UDim2::fromScale(1.0f, 1.0f),
        });

        m_icon = row->add<ImageLabel>();
        m_icon->setSvg(Icons::ARROW);
        m_icon->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
        m_icon->setBaseProperties({
            .anchorPoint = {1.0f, 0.5f},
            .interactable = false,
            .position = UDim2(1.0f, 0.0f, 0.5f, 0.0f),
            .visible = true,
            .zIndex = POPUP_ZINDEX,
        });
        return row;
    }

    void bind(ContextMenu::ItemData &item) override
    {
        m_boundItem = &item;
        auto &submenu = item.as<ContextMenuSubmenu>();
        m_label->setText(submenu.label);
        m_row->setBaseProperties({.interactable = submenu.enabled});

        float iconSize = m_owner->itemHeight - ICON_INSET;
        m_icon->setImageStyleProperties({.imageColor = m_owner->getTextStyleProperties().textColor});
        m_icon->setBaseProperties({.size = UDim2::fromOffset(iconSize, iconSize)});
    }

  private:
    TextLabel *m_label = nullptr;
    ImageLabel *m_icon = nullptr;
};

class DefaultRadioItemView : public ContextMenu::RadioItemView {
  public:
    Frame *create(ContextMenu &owner) override
    {
        Frame *row = RadioItemView::create(owner);
        m_label = row->add<TextLabel>();
        m_label->setTextStyleProperties(owner.getTextStyleProperties());
        m_label->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
        m_label->setBaseProperties({
            .interactable = false,
            .padding =
                UDim4{UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING), UDim::fromOffset(0.0f), UDim::fromOffset(ROW_PADDING)},
            .size = UDim2::fromScale(1.0f, 1.0f),
        });

        m_radio = row->add<RadioButton>();
        m_radio->setBaseProperties({
            .anchorPoint = {1.0f, 0.5f},
            .position = UDim2(1.0f, 0.0f, 0.5f, 0.0f),
            .zIndex = POPUP_ZINDEX,
        });
        return row;
    }

    void bind(ContextMenu::ItemData &item) override
    {
        m_boundItem = &item;
        auto &radioItem = item.as<ContextMenuRadio>();
        m_label->setText(s_buildItemText(radioItem.label, radioItem.shortcutHint));
        m_row->setBaseProperties({.interactable = radioItem.enabled});

        float iconSize = m_owner->itemHeight - ICON_INSET;
        m_radio->setBaseProperties({.size = UDim2::fromOffset(iconSize, iconSize)});
        m_radio->value = radioItem.value;
        m_radio->setGroup(radioItem.group);
    }

  private:
    TextLabel *m_label = nullptr;
    RadioButton *m_radio = nullptr;
};

ContextMenu::ContextMenu()
{
    closeOnClickOutside = false;
    placement = PopupPlacement::BELOW;
    maxVisibleItems = 8;
    itemHeight = 24.0f;
    popupWidth = 180.0f;
    setTextStyleProperties({.textXAlignment = TextXAlignment::LEFT, .textYAlignment = TextYAlignment::CENTER});
    // presses landing between rows stop at the menu too
    consume(INTERACTION_CATEGORY_CLICK);
    m_overlayPtr = nullptr;

    m_rowFactories.action = [] { return std::make_unique<DefaultActionItemView>(); };
    m_rowFactories.toggle = [] { return std::make_unique<DefaultToggleItemView>(); };
    m_rowFactories.separator = [] { return std::make_unique<DefaultSeparatorItemView>(); };
    m_rowFactories.submenu = [] { return std::make_unique<DefaultSubmenuItemView>(); };
    m_rowFactories.radio = [] { return std::make_unique<DefaultRadioItemView>(); };

    resolveStyle();
}

void ContextMenu::resolveStyle()
{
    resolveBaseStyle(ComponentType::CONTEXT_MENU);

    ContextMenuStyleProperties resolved =
        Style::instance().getContextMenuStyle(ComponentType::CONTEXT_MENU, getClasses(), effectiveGuiState());
    if (m_cmProps.apply(resolved)) {
        markDirty();
    }

    TextStyleProperties resolvedText =
        Style::instance().getTextStyle(ComponentType::CONTEXT_MENU, getClasses(), effectiveGuiState());
    if (m_textProps.apply(resolvedText)) {
        markDirty();
    }
}

bool ContextMenu::setContextMenuProperties(const ContextMenuStylePropertiesArgs &props)
{
    bool changed = m_cmProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

bool ContextMenu::setTextStyleProperties(const TextStylePropertiesArgs &props)
{
    bool changed = m_textProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

void ContextMenu::setItems(std::vector<std::unique_ptr<ItemData>> items)
{
    if (isOpen()) {
        hide();
    }
    m_items = std::move(items);
    m_itemsPtr = &m_items;
}

void ContextMenu::setRowFactories(RowFactories factories)
{
    if (factories.action) m_rowFactories.action = std::move(factories.action);
    if (factories.toggle) m_rowFactories.toggle = std::move(factories.toggle);
    if (factories.separator) m_rowFactories.separator = std::move(factories.separator);
    if (factories.submenu) m_rowFactories.submenu = std::move(factories.submenu);
    if (factories.radio) m_rowFactories.radio = std::move(factories.radio);
}

bool ContextMenu::prepareShow()
{
    if (isOpen()) {
        return false;
    }
    Window *win = getWindow();
    if (win == nullptr) {
        return false;
    }
    m_overlayPtr = win->getOverlayLayer();
    if (m_overlayPtr == nullptr) {
        return false;
    }

    buildMainContent();

    if (!m_pressConn.connected()) {
        m_pressConn = m_overlayPtr->onPressVote.connect([this](vec2 pos, PressVote &vote) {
            if (!isOpen()) {
                return;
            }
            bool inside = containsPoint(pos);
            if (!inside && m_submenu != nullptr && m_submenu->isOpen() && m_submenu->containsPoint(pos)) {
                inside = true;
            }
            if (inside) {
                vote.add(EventResult::PROPAGATE);
                return;
            }
            vote.add(EventResult::CONSUMED);
            hide();
        });
    }

    return true;
}

void ContextMenu::show(UIObject *anchor)
{
    if (!prepareShow()) {
        return;
    }

    Popup::open(anchor);
    if (onOpenedCb) {
        onOpenedCb();
    }
}

void ContextMenu::showAt(vec2 pos)
{
    if (!prepareShow()) {
        return;
    }

    Popup::openAt(pos);
    if (onOpenedCb) {
        onOpenedCb();
    }
}

void ContextMenu::hide()
{
    if (!isOpen()) {
        return;
    }
    closeSubmenu();
    Popup::close();
    if (onClosedCb) {
        onClosedCb();
    }
}

void ContextMenu::closeSubmenu()
{
    if (m_submenuSourceRow != nullptr) {
        m_submenuSourceRow->setBaseStyleProperties({.backgroundColor = getBaseStyleProperties().backgroundColor});
        m_submenuSourceRow = nullptr;
    }
    if (m_submenu != nullptr) {
        m_submenu->hide();
    }
}

void ContextMenu::openSubmenu(SubmenuItemData &submenu, Frame *sourceRow)
{
    closeSubmenu();

    if (m_submenu == nullptr) {
        m_submenu = add<ContextMenu>();
        m_submenu->closeOnClickOutside = false;
    }

    m_submenu->setBaseStyleProperties(getBaseStyleProperties());
    m_submenu->m_itemsPtr = &submenu.items;
    m_submenu->placement = PopupPlacement::RIGHT;
    m_submenuSourceRow = sourceRow;

    m_submenu->show(sourceRow);
}

void ContextMenu::buildMainContent()
{
    // TODO: pool/recycle views and window off scroll offset instead of building one per item every show() (see
    // docs/amethyst/context_menu_plan.md)
    removeAllChildren();
    m_views.clear();
    m_submenu = nullptr;
    m_submenuSourceRow = nullptr;

    std::vector<std::unique_ptr<ItemData>> &itemList = items();

    std::vector<std::unique_ptr<ItemView>> views;
    views.reserve(itemList.size());
    for (auto &itemPtr : itemList) {
        std::function<std::unique_ptr<ItemView>()> *factory = nullptr;
        switch (itemPtr->kind) {
        case Kind::ACTION:
            factory = &m_rowFactories.action;
            break;
        case Kind::TOGGLE:
            factory = &m_rowFactories.toggle;
            break;
        case Kind::SEPARATOR:
            factory = &m_rowFactories.separator;
            break;
        case Kind::SUBMENU:
            factory = &m_rowFactories.submenu;
            break;
        case Kind::RADIO:
            factory = &m_rowFactories.radio;
            break;
        }
        views.push_back((*factory)());
    }

    float totalHeight = 0.0f;
    for (auto &view : views) {
        totalHeight += view->rowHeight(*this);
    }
    float visibleHeight = totalHeight;
    if (maxVisibleItems != INT_MAX) {
        visibleHeight = std::min(visibleHeight, static_cast<float>(maxVisibleItems) * itemHeight);
    }
    if (maxContentHeight > 0.0f) {
        visibleHeight = std::min(visibleHeight, maxContentHeight);
    }

    setBaseProperties({
        .clipsDescendants = true,
        .size = UDim2::fromOffset(popupWidth, visibleHeight),
        .zIndex = POPUP_ZINDEX,
    });

    Instance *container = this;
    if (totalHeight > visibleHeight + 0.5f) {
        auto *sf = add<ScrollingFrame>();
        sf->setBaseStyleProperties(getBaseStyleProperties());
        sf->setBaseProperties({.clipsDescendants = true, .size = UDim2::fromScale(1.0f, 1.0f)});
        sf->setScrollingFrameProperties({
            .scrollAxis = ScrollAxis::Y,
            .scrollBarVisibility = ScrollBarVisibility::AUTO,
            .canvasSize = UDim2::fromOffset(popupWidth, totalHeight),
        });
        container = sf;
    }

    float y = 0.0f;
    for (size_t idx = 0; idx < itemList.size(); idx++) {
        ItemView &view = *views[idx];
        float height = view.rowHeight(*this);
        Frame *row = view.create(*this);
        if (container != this) {
            row->reparent(container);
        }
        row->setBaseProperties({
            .position = UDim2::fromOffset(0.0f, y),
            .size = UDim2(1.0f, 0.0f, 0.0f, height),
            .zIndex = POPUP_ZINDEX,
        });
        row->addClasses(getClasses());
        row->bindPart(s_partForKind(itemList[idx]->kind));
        row->propagateClassesToChildren();
        view.bind(*itemList[idx]);
        y += height;
    }
    m_views = std::move(views);

    markDirty();
}

void ContextMenu::ActionItemView::activate(ContextMenu &owner, ActionItemData &action)
{
    if (action.onActivate) {
        action.onActivate();
    }
    owner.hide();
}

void ContextMenu::ToggleItemView::toggle(ToggleItemData &item)
{
    item.value = !item.value;
    if (item.onToggled) {
        item.onToggled(item.value);
    }
}

void ContextMenu::SubmenuItemView::openSubmenu(ContextMenu &owner, SubmenuItemData &submenu, Frame *sourceRow)
{
    owner.openSubmenu(submenu, sourceRow);
}

void ContextMenu::RadioItemView::select(RadioItemData &item)
{
    if (item.group != nullptr) {
        item.group->select(item.value);
    }
}

} // namespace Amethyst
