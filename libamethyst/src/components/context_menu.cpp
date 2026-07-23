#include "components/context_menu.h"

#include "amethyst/icons.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "components/image_label.h"
#include "components/overlay_layer.h"
#include "components/scrolling_frame.h"
#include "components/text_button.h"
#include "components/window.h"
#include "modules/style.h"

#include "math/math.h"
#include <algorithm>
#include <climits>

namespace Amethyst {

#define POPUP_ZINDEX 10

static constexpr float ICON_INSET = 4.0f;
static constexpr float ROW_PADDING = 8.0f;

ContextMenu::ContextMenu()
{
    closeOnClickOutside = false;
    setBaseStyleProperties({
        .backgroundColor = Color3{0.18f, 0.18f, 0.18f},
        .backgroundTransparency = 0.0f,
        .borderPixelSize = 0.0f,
    });
    maxVisibleItems = 8;
    itemHeight = 24.0f;
    popupWidth = 180.0f;
    m_textProps.textXAlignment = TextXAlignment::LEFT;
    m_textProps.textYAlignment = TextYAlignment::CENTER;
    m_overlayPtr = nullptr;

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

void ContextMenu::setItems(std::vector<ContextMenuItem> items)
{
    if (isOpen()) {
        hide();
    }
    m_items = std::move(items);
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
            for (size_t i = 0; !inside && i < m_submenuStack.size(); i++) {
                if (m_submenuStack[i] != nullptr && m_submenuStack[i]->isOpen() && m_submenuStack[i]->containsPoint(pos)) {
                    inside = true;
                }
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
    closeSubmenuFrom(0);
    Popup::close();
    if (onClosedCb) {
        onClosedCb();
    }
}

void ContextMenu::closeSubmenuFrom(size_t depth)
{
    for (size_t i = depth; i < m_submenuSourceRows.size(); i++) {
        if (m_submenuSourceRows[i] != nullptr) {
            m_submenuSourceRows[i]->setBaseStyleProperties({.backgroundColor = getBaseStyleProperties().backgroundColor});
        }
    }
    m_submenuSourceRows.resize(depth);

    for (size_t i = depth; i < m_submenuStack.size(); i++) {
        if (m_submenuStack[i] != nullptr) {
            m_submenuStack[i]->hide();
        }
    }
    m_submenuStack.resize(depth);
}

float ContextMenu::computeTotalHeight(const std::vector<ContextMenuItem> &items) const
{
    float h = 0.0f;
    for (auto &item : items) {
        h += (item.kind() == ContextMenuItem::Kind::SEPARATOR) ? 8.0f : itemHeight;
    }
    return h;
}

void ContextMenu::buildMainContent()
{
    buildContent(this, {});
    placement = PopupPlacement::BELOW;
}

void ContextMenu::buildSubmenuAtPath(const std::vector<size_t> &path, UIObject *sourceRow)
{
    size_t depth = path.size() - 1;
    closeSubmenuFrom(depth);

    if (m_submenuStack.size() <= depth) {
        m_submenuStack.resize(depth + 1, nullptr);
    }

    if (m_submenuStack[depth] == nullptr) {
        m_submenuStack[depth] = add<ContextMenu>();
        m_submenuStack[depth]->closeOnClickOutside = false;
        m_submenuStack[depth]->setBaseStyleProperties(getBaseStyleProperties());
    }

    buildContent(m_submenuStack[depth], path);
    m_submenuStack[depth]->placement = PopupPlacement::RIGHT;
    m_submenuStack[depth]->Popup::open(sourceRow);
}

void ContextMenu::buildContent(Popup *popup, const std::vector<size_t> &path)
{
    std::vector<ContextMenuItem> &items = itemsAtPath(path);
    float totalHeight = computeTotalHeight(items);
    float visibleHeight =
        (maxVisibleItems == INT_MAX) ? totalHeight : std::min(totalHeight, static_cast<float>(maxVisibleItems) * itemHeight);

    popup->removeAllChildren();
    popup->setBaseProperties({
        .clipsDescendants = true,
        .size = UDim2::fromOffset(popupWidth, visibleHeight),
        .zIndex = POPUP_ZINDEX,
    });

    UIObject *container = popup;
    if (totalHeight > visibleHeight + 0.5f) {
        popup->removeExtension<UIListLayout>();
        auto *sf = popup->add<ScrollingFrame>();
        sf->setBaseStyleProperties(getBaseStyleProperties());
        sf->setBaseProperties({
            .clipsDescendants = true,
            .size = UDim2::fromScale(1.0f, 1.0f),
        });
        sf->setScrollingFrameProperties({
            .scrollAxis = ScrollAxis::Y,
            .scrollBarVisibility = ScrollBarVisibility::AUTO,
            .canvasSize = UDim2::fromOffset(popupWidth, totalHeight),
        });
        container = sf;
    }

    auto *layout = container->addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_VERTICAL;
    layout->horizontalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;

    addItemRows(container, path);
    popup->markDirty();
}

void ContextMenu::addItemRows(Instance *container, const std::vector<size_t> &path)
{
    std::vector<ContextMenuItem> &items = itemsAtPath(path);
    size_t depth = path.size();

    Color3 bgColor = getBaseStyleProperties().backgroundColor;
    float iconSize = itemHeight - ICON_INSET;

    for (size_t idx = 0; idx < items.size(); idx++) {
        ContextMenuItem &item = items[idx];

        if (item.kind() == ContextMenuItem::Kind::SEPARATOR) {
            auto *sep = container->add<Frame>();
            sep->setBaseStyleProperties({
                .backgroundColor = m_cmProps.separatorColor,
                .backgroundTransparency = 0.5f,
                .borderPixelSize = 0.0f,
            });
            sep->setBaseProperties({
                .interactable = false,
                .layoutOrder = static_cast<LayoutOrder>(idx * 100),
                .size = UDim2::fromOffset(popupWidth, 8.0f),
                .zIndex = POPUP_ZINDEX,
            });
            continue;
        }

        bool hasIcon = item.kind() == ContextMenuItem::Kind::TOGGLE || item.kind() == ContextMenuItem::Kind::SUBMENU;

        auto *row = container->add<TextButton>();
        row->setBaseStyleProperties({
            .backgroundColor = bgColor,
            .backgroundTransparency = 0.0f,
            .borderPixelSize = 0.0f,
        });
        row->setBaseProperties({
            .layoutOrder = static_cast<LayoutOrder>(idx * 100),
            .padding = UDim4::fromOffset(ROW_PADDING),
            .size = UDim2::fromOffset(popupWidth, itemHeight),
            .zIndex = POPUP_ZINDEX,
        });
        row->setButtonProperties({.autoButtonColor = false});
        row->setTextStyleProperties(m_textProps);
        row->setText(buildItemText(item));

        ImageLabel *checkIcon = nullptr;
        if (hasIcon) {
            bool isToggle = item.kind() == ContextMenuItem::Kind::TOGGLE;
            auto *icon = row->add<ImageLabel>();
            icon->setSvg(isToggle ? Icons::CHECK : Icons::ARROW);
            icon->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});
            icon->setImageStyleProperties({.imageColor = m_textProps.textColor});
            icon->setBaseProperties({
                .anchorPoint = {1.0f, 0.5f},
                .interactable = false,
                .position = UDim2(1.0f, 0.0f, 0.5f, 0.0f),
                .size = UDim2::fromOffset(iconSize, iconSize),
                .visible = isToggle ? std::get<ContextMenuToggle>(item.payload).currentState() : true,
                .zIndex = POPUP_ZINDEX,
            });
            if (isToggle) {
                checkIcon = icon;
            }
        }

        if (!item.enabled) {
            row->setBaseProperties({.interactable = false});
            continue;
        }

        Color3 hoverBg = m_cmProps.itemHoverBackground;

        row->onMouseLeaveCb = [this, row, bgColor, hoverBg, depth]() {
            bool isSubmenuSource = depth < m_submenuSourceRows.size() && m_submenuSourceRows[depth] == row;
            Color3 newBg = isSubmenuSource ? hoverBg : bgColor;
            if (row->getBaseStyleProperties().backgroundColor != newBg) {
                row->setBaseStyleProperties({.backgroundColor = newBg});
            }
            return EventResult::CONSUMED;
        };

        if (item.kind() == ContextMenuItem::Kind::SUBMENU) {
            std::vector<size_t> fullPath = path;
            fullPath.push_back(idx);
            row->onMouseEnterCb = [this, row, hoverBg, fullPath = std::move(fullPath)]() {
                if (row->getBaseStyleProperties().backgroundColor != hoverBg) {
                    row->setBaseStyleProperties({.backgroundColor = hoverBg});
                }
                buildSubmenuAtPath(fullPath, row);
                m_submenuSourceRows.push_back(row);
                return EventResult::CONSUMED;
            };
            row->onMouseButton1ClickCb = []() { return EventResult::CONSUMED; };
        } else {
            row->onMouseEnterCb = [this, row, hoverBg, depth]() {
                if (row->getBaseStyleProperties().backgroundColor != hoverBg) {
                    row->setBaseStyleProperties({.backgroundColor = hoverBg});
                }
                closeSubmenuFrom(depth);
                return EventResult::CONSUMED;
            };
        }

        if (item.kind() == ContextMenuItem::Kind::ACTION) {
            std::function<void()> cb = std::get<ContextMenuAction>(item.payload).onActivate;
            row->onMouseButton1ClickCb = [this, cb]() {
                if (cb) {
                    cb();
                }
                hide();
                return EventResult::CONSUMED;
            };
        } else if (item.kind() == ContextMenuItem::Kind::SELECT) {
            std::string label = item.label;
            row->onMouseButton1ClickCb = [this, label]() {
                if (onItemSelected) {
                    onItemSelected(label);
                }
                hide();
                return EventResult::CONSUMED;
            };
        } else if (item.kind() == ContextMenuItem::Kind::TOGGLE) {
            ContextMenuItem *itemPtr = &items[idx];
            row->onMouseButton1ClickCb = [itemPtr, checkIcon]() {
                auto &toggle = std::get<ContextMenuToggle>(itemPtr->payload);
                toggle.toggle();
                if (checkIcon != nullptr) {
                    checkIcon->setBaseProperties({.visible = toggle.currentState()});
                }
                return EventResult::CONSUMED;
            };
        }
    }
}

std::vector<ContextMenuItem> &ContextMenu::itemsAtPath(const std::vector<size_t> &path)
{
    std::vector<ContextMenuItem> *items = &m_items;
    for (size_t idx : path) {
        items = &std::get<ContextMenuSubmenu>((*items)[idx].payload).items;
    }
    return *items;
}

std::string ContextMenu::buildItemText(const ContextMenuItem &item) const
{
    std::string text;
    text.reserve(item.label.size() + item.shortcutHint.size() + 8);
    text.append(item.label);
    if (!item.shortcutHint.empty()) {
        text.append("    ");
        text.append(item.shortcutHint);
    }
    return text;
}

} // namespace Amethyst
