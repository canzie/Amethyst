#include "components/dropdown.h"

#include "components/dropdown_item.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "components/overlay_layer.h"
#include "components/popup.h"
#include "components/scrolling_frame.h"
#include "components/window.h"

#include "math/math.h"
#include <algorithm>
#include <climits>

namespace Amethyst {

static PopupPlacement s_placementFor(DropdownDirection dir)
{
    switch (dir) {
    case DropdownDirection::UP:
        return PopupPlacement::ABOVE;
    case DropdownDirection::LEFT:
        return PopupPlacement::LEFT;
    case DropdownDirection::RIGHT:
        return PopupPlacement::RIGHT;
    default:
        return PopupPlacement::BELOW;
    }
}

Dropdown::Dropdown()
{
    m_ddProps.popupDirection = DropdownDirection::DOWN;
    m_ddProps.maxVisibleItems = 8;
    m_ddProps.itemHeight = 24.0f;
    m_ddProps.popupWidth = 180.0f;
    m_ddProps.itemFontSize = 14.0f;
    m_ddProps.popupBackground = Color3{0.18f, 0.18f, 0.18f};
    m_ddProps.itemTextColor = Color4{0.92f, 0.92f, 0.92f, 1.0f};
    m_ddProps.itemDisabledColor = Color4{0.45f, 0.45f, 0.45f, 1.0f};
    m_ddProps.itemHoverBackground = Color3{0.25f, 0.42f, 0.65f};
    m_ddProps.separatorColor = Color3{0.32f, 0.32f, 0.32f};
}

bool Dropdown::setDropdownProperties(const DropdownStyleProperties &props)
{
    bool changed = m_ddProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

Dropdown::~Dropdown()
{
    if (m_overlayPtr != nullptr) {
        for (Popup *sub : m_submenuStack) {
            if (sub != nullptr) {
                m_overlayPtr->removeChild(sub);
            }
        }
        if (m_popup != nullptr) {
            m_overlayPtr->removeChild(m_popup);
        }
    }
}

void Dropdown::setItems(std::vector<DropdownItem> items)
{
    if (m_open) {
        requestClose();
    }
    m_items = std::move(items);
}

void Dropdown::open()
{
    if (m_open) {
        return;
    }
    Window *win = getWindow();
    if (win == nullptr) {
        return;
    }
    m_overlayPtr = win->getOverlayLayer();
    if (m_overlayPtr == nullptr) {
        return;
    }

    buildMainPopup();

    if (!m_pressConn.connected()) {
        m_pressConn = m_overlayPtr->onPressVote.connect([this](vec2 pos, PressVote &vote) {
            if (!m_open) {
                return;
            }
            bool inside = m_popup != nullptr && m_popup->containsPoint(pos);
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
            requestClose();
        });
    }

    m_open = true;
    if (onOpenedCb) {
        onOpenedCb();
    }
}

void Dropdown::requestClose()
{
    if (!m_open) {
        return;
    }
    closeSubmenuFrom(0);
    if (m_popup != nullptr) {
        m_popup->close();
    }
    m_open = false;
    if (onClosedCb) {
        onClosedCb();
    }
}

void Dropdown::closeImmediate()
{
    requestClose();
}

void Dropdown::closeSubmenuFrom(size_t depth)
{
    for (size_t i = depth; i < m_submenuSourceRows.size(); i++) {
        if (m_submenuSourceRows[i] != nullptr) {
            m_submenuSourceRows[i]->setBaseStyleProperties({.backgroundColor = m_ddProps.popupBackground});
        }
    }
    m_submenuSourceRows.resize(depth);

    for (size_t i = depth; i < m_submenuStack.size(); i++) {
        if (m_submenuStack[i] != nullptr) {
            m_submenuStack[i]->close();
        }
    }
}

float Dropdown::computeTotalHeight(const std::vector<DropdownItem> &items) const
{
    float h = 0.0f;
    for (auto &item : items) {
        h += (item.kind() == DropdownItem::Kind::SEPARATOR) ? 8.0f : m_ddProps.itemHeight;
    }
    return h;
}

void Dropdown::buildMainPopup()
{
    float totalHeight = computeTotalHeight(m_items);
    float visibleHeight = (m_ddProps.maxVisibleItems == INT_MAX)
                              ? totalHeight
                              : std::min(totalHeight, static_cast<float>(m_ddProps.maxVisibleItems) * m_ddProps.itemHeight);

    buildPopupPanel(m_popup, totalHeight, visibleHeight, 1, {});
    m_popup->placement = s_placementFor(m_ddProps.popupDirection);
    m_popup->open(this);
}

void Dropdown::buildSubmenuAtPath(const std::vector<size_t> &path, UIObject *sourceRow)
{
    size_t depth = path.size() - 1;
    closeSubmenuFrom(depth);

    std::vector<DropdownItem> &subItems = itemsAtPath(path);
    float totalHeight = computeTotalHeight(subItems);
    float visibleHeight = (m_ddProps.maxVisibleItems == INT_MAX)
                              ? totalHeight
                              : std::min(totalHeight, static_cast<float>(m_ddProps.maxVisibleItems) * m_ddProps.itemHeight);

    if (m_submenuStack.size() <= depth) {
        m_submenuStack.resize(depth + 1, nullptr);
    }

    buildPopupPanel(m_submenuStack[depth], totalHeight, visibleHeight, 2 + static_cast<int>(depth), path);
    m_submenuStack[depth]->placement = PopupPlacement::RIGHT;
    m_submenuStack[depth]->open(sourceRow);
}

std::vector<DropdownItem> &Dropdown::itemsAtPath(const std::vector<size_t> &path)
{
    std::vector<DropdownItem> *items = &m_items;
    for (size_t idx : path) {
        items = &std::get<DropdownSubmenu>((*items)[idx].payload).items;
    }
    return *items;
}

Popup *Dropdown::buildPopupPanel(Popup *&slot, float totalHeight, float visibleHeight, int zIdx, const std::vector<size_t> &path)
{
    if (slot == nullptr) {
        auto popup = std::make_unique<Popup>();
        slot = static_cast<Popup *>(m_overlayPtr->addChild(std::move(popup)));
        slot->closeOnClickOutside = false;
    }

    slot->removeAllChildren();
    slot->setBaseStyleProperties({
        .backgroundColor = m_ddProps.popupBackground,
        .backgroundTransparency = 0.0f,
        .borderPixelSize = 0.0f,
    });
    slot->setBaseProperties({
        .clipsDescendants = true,
        .size = UDim2::fromOffset(m_ddProps.popupWidth, visibleHeight),
        .zIndex = zIdx,
    });

    UIObject *container = slot;
    if (totalHeight > visibleHeight + 0.5f) {
        slot->removeExtension<UIListLayout>();
        auto *sf = slot->add<ScrollingFrame>();
        sf->setBaseStyleProperties({
            .backgroundColor = m_ddProps.popupBackground,
            .backgroundTransparency = 0.0f,
            .borderPixelSize = 0.0f,
        });
        sf->setBaseProperties({
            .clipsDescendants = true,
            .size = UDim2::fromScale(1.0f, 1.0f),
        });
        sf->setScrollingFrameProperties({
            .scrollAxis = ScrollAxis::Y,
            .scrollBarVisibility = ScrollBarVisibility::AUTO,
            .canvasSize = UDim2::fromOffset(m_ddProps.popupWidth, totalHeight),
        });
        container = sf;
    }

    auto *layout = container->addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_VERTICAL;
    layout->horizontalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;

    addItemRows(container, zIdx + 1, path);
    slot->markDirty();
    return slot;
}

void Dropdown::addItemRows(Instance *container, int zIdx, const std::vector<size_t> &path)
{
    std::vector<DropdownItem> &items = itemsAtPath(path);
    size_t depth = path.size();

    for (size_t idx = 0; idx < items.size(); idx++) {
        DropdownItem &item = items[idx];

        if (item.kind() == DropdownItem::Kind::SEPARATOR) {
            auto *sep = container->add<Frame>();
            sep->setBaseStyleProperties({
                .backgroundColor = m_ddProps.separatorColor,
                .backgroundTransparency = 0.5f,
                .borderPixelSize = 0.0f,
            });
            sep->setBaseProperties({
                .interactable = false,
                .layoutOrder = static_cast<LayoutOrder>(idx * 100),
                .size = UDim2::fromOffset(m_ddProps.popupWidth, 8.0f),
                .zIndex = zIdx,
            });
            continue;
        }

        auto *row = container->add<TextButton>();
        row->setBaseStyleProperties({
            .backgroundColor = m_ddProps.popupBackground,
            .backgroundTransparency = 0.0f,
            .borderPixelSize = 0.0f,
        });
        row->setBaseProperties({
            .layoutOrder = static_cast<LayoutOrder>(idx * 100),
            .padding = {UDim::fromOffset(0.0f), UDim::fromOffset(8.0f), UDim::fromOffset(0.0f), UDim::fromOffset(8.0f)},
            .size = UDim2::fromOffset(m_ddProps.popupWidth, m_ddProps.itemHeight),
            .zIndex = zIdx,
        });
        row->setButtonProperties({.autoButtonColor = false});
        row->setTextStyleProperties({
            .fontSize = m_ddProps.itemFontSize,
            .textColor = item.enabled ? m_ddProps.itemTextColor : m_ddProps.itemDisabledColor,
            .textXAlignment = TextXAlignment::LEFT,
            .textYAlignment = TextYAlignment::CENTER,
            .textWrapped = false,
        });
        row->setText(buildItemText(item));

        if (!item.enabled) {
            row->setBaseProperties({.interactable = false});
            continue;
        }

        Color3 hoverBg = m_ddProps.itemHoverBackground;
        Color3 normalBg = m_ddProps.popupBackground;

        row->onMouseLeaveCb = [this, row, normalBg, hoverBg, depth]() {
            bool isSubmenuSource = depth < m_submenuSourceRows.size() && m_submenuSourceRows[depth] == row;
            Color3 newBg = isSubmenuSource ? hoverBg : normalBg;
            if (row->getBaseStyleProperties().backgroundColor != newBg) {
                row->setBaseStyleProperties({.backgroundColor = newBg});
            }
            return EventResult::CONSUMED;
        };

        if (item.kind() == DropdownItem::Kind::SUBMENU) {
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

        if (item.kind() == DropdownItem::Kind::ACTION) {
            std::function<void()> cb = std::get<DropdownAction>(item.payload).onActivate;
            row->onMouseButton1ClickCb = [this, cb]() {
                if (cb) {
                    cb();
                }
                requestClose();
                return EventResult::CONSUMED;
            };
        } else if (item.kind() == DropdownItem::Kind::SELECT) {
            std::string label = item.label;
            row->onMouseButton1ClickCb = [this, label]() {
                if (onItemSelected) {
                    onItemSelected(label);
                }
                requestClose();
                return EventResult::CONSUMED;
            };
        } else if (item.kind() == DropdownItem::Kind::TOGGLE) {
            DropdownItem *itemPtr = &items[idx];
            row->onMouseButton1ClickCb = [this, row, itemPtr]() {
                std::get<DropdownToggle>(itemPtr->payload).toggle();
                row->setText(buildItemText(*itemPtr));
                return EventResult::CONSUMED;
            };
        }
    }
}

std::string Dropdown::buildItemText(const DropdownItem &item) const
{
    std::string text;
    text.reserve(item.label.size() + item.shortcutHint.size() + 8);
    if (item.kind() == DropdownItem::Kind::TOGGLE) {
        const auto &t = std::get<DropdownToggle>(item.payload);
        text.append(t.currentState() ? "\xe2\x9c\x93 " : "  ");
    }
    text.append(item.label);
    if (item.kind() == DropdownItem::Kind::SUBMENU) {
        text.append(" \xe2\x96\xb6");
    }
    if (!item.shortcutHint.empty()) {
        text.append("    ");
        text.append(item.shortcutHint);
    }
    return text;
}

EventResult Dropdown::onMouseButton1Down(uint32_t x, uint32_t y)
{
    UIButton::onMouseButton1Down(x, y);
    if (!m_open) {
        open();
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
