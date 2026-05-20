#include "components/dropdown.h"

#include "components/dropdown_item.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/overlay_layer.h"
#include "components/scrolling_frame.h"
#include "components/window.h"
#include "rendering/draw_context.h"

#include <climits>
#include <glm/glm.hpp>

namespace Amethyst {

Dropdown::Dropdown() = default;

Dropdown::~Dropdown()
{
    // TODO(C4): once a signal/event system is in place, unsubscribe from the
    // overlay's destroy signal here so m_overlayPtr is guaranteed valid.
    // For now m_overlayPtr may already be null if the Window destroyed its
    // OverlayLayer before its children — actuallyClose() null-checks it.
    if (m_state != State::CLOSED) {
        onClosedCb = nullptr;
        actuallyClose();
    }
}

void Dropdown::setItems(std::vector<DropdownItem> items)
{
    if (m_state != State::CLOSED) {
        actuallyClose();
    }
    m_items = std::move(items);
}

void Dropdown::open()
{
    if (m_state != State::CLOSED) {
        return;
    }
    Window *win = getWindow();
    if (!win) {
        return;
    }

    m_overlayPtr = win->getOverlayLayer();

    auto eater = std::make_unique<InvisibleButton>();
    eater->size = UDim2::fromScale(1.0f, 1.0f);
    eater->zIndex = 0;
    eater->onMouseButton1DownCb = [this](uint32_t, uint32_t) {
        requestClose();
        return EventResult::CONSUMED;
    };
    m_eater = static_cast<InvisibleButton *>(m_overlayPtr->addChild(std::move(eater)));

    buildMainPopup(m_overlayPtr);

    m_state = State::OPEN;

    if (onOpenedCb) {
        onOpenedCb();
    }
}

void Dropdown::requestClose()
{
    if (m_state != State::OPEN) {
        return;
    }
    m_state = State::PENDING_CLOSE;
    if (m_popup) {
        m_popup->visible = false;
        m_popup->markDirty();
    }
    if (m_submenu) {
        m_submenu->visible = false;
        m_submenu->markDirty();
    }
    if (m_eater) {
        m_eater->visible = false;
        m_eater->interactable = false;
    }
    markDirty();
}

void Dropdown::actuallyClose()
{
    m_submenuSourceRow = nullptr;
    if (m_overlayPtr) {
        if (m_submenu) {
            m_overlayPtr->removeChild(m_submenu);
        }
        if (m_popup) {
            m_overlayPtr->removeChild(m_popup);
        }
        if (m_eater) {
            m_overlayPtr->removeChild(m_eater);
        }
        m_overlayPtr = nullptr;
    }
    m_submenu = nullptr;
    m_popup = nullptr;
    m_eater = nullptr;
    m_state = State::CLOSED;

    if (onClosedCb) {
        onClosedCb();
    }
}

void Dropdown::closeSubmenu()
{
    if (m_submenuSourceRow) {
        m_submenuSourceRow->backgroundColor = popupBackground;
        m_submenuSourceRow->markDirty();
        m_submenuSourceRow = nullptr;
    }
    if (m_submenu && m_overlayPtr) {
        m_overlayPtr->removeChild(m_submenu);
        m_submenu = nullptr;
    }
}

float Dropdown::computeTotalHeight(const std::vector<DropdownItem> &items) const
{
    float h = 0.0f;
    for (auto &item : items) {
        h += (item.kind() == DropdownItem::Kind::SEPARATOR) ? 8.0f : itemHeight;
    }
    return h;
}

void Dropdown::buildMainPopup(OverlayLayer *overlay)
{
    float totalHeight = computeTotalHeight(m_items);
    float visibleHeight = (maxVisibleItems == INT_MAX)
        ? totalHeight
        : std::min(totalHeight, static_cast<float>(maxVisibleItems) * itemHeight);

    glm::vec2 pos;
    switch (popupDirection) {
    case DropdownDirection::UP:
        pos = {absolutePosition.x, absolutePosition.y - visibleHeight};
        break;
    case DropdownDirection::LEFT:
        pos = {absolutePosition.x - popupWidth, absolutePosition.y};
        break;
    case DropdownDirection::RIGHT:
        pos = {absolutePosition.x + absoluteSize.x, absolutePosition.y};
        break;
    default:
        pos = {absolutePosition.x, absolutePosition.y + absoluteSize.y};
        break;
    }

    m_popup = buildPopupPanel(overlay, &m_items, pos, totalHeight, visibleHeight, 1, false);
}

void Dropdown::buildSubmenuPopup(OverlayLayer *overlay, std::vector<DropdownItem> *items, glm::vec2 pos)
{
    closeSubmenu();
    float totalHeight = computeTotalHeight(*items);
    m_submenu = buildPopupPanel(overlay, items, pos, totalHeight, totalHeight, 2, true);
}

UIObject *Dropdown::buildPopupPanel(OverlayLayer *overlay, std::vector<DropdownItem> *items,
                                     glm::vec2 pos, float totalHeight, float visibleHeight, int zIdx, bool inSubmenu)
{
    UIObject *panel;

    if (totalHeight > visibleHeight + 0.5f) {
        auto sf = std::make_unique<ScrollingFrame>();
        sf->position = UDim2::fromOffset(pos.x, pos.y);
        sf->size = UDim2::fromOffset(popupWidth, visibleHeight);
        sf->canvasSize = UDim2::fromOffset(popupWidth, totalHeight);
        sf->scrollAxis = ScrollAxis::Y;
        sf->scrollBarVisibility = ScrollBarVisibility::AUTO;
        sf->backgroundColor = popupBackground;
        sf->backgroundTransparency = 0.0f;
        sf->borderPixelSize = 0.0f;
        sf->zIndex = zIdx;
        panel = static_cast<UIObject *>(overlay->addChild(std::move(sf)));
    } else {
        auto fr = std::make_unique<Frame>();
        fr->position = UDim2::fromOffset(pos.x, pos.y);
        fr->size = UDim2::fromOffset(popupWidth, visibleHeight);
        fr->backgroundColor = popupBackground;
        fr->backgroundTransparency = 0.0f;
        fr->borderPixelSize = 0.0f;
        fr->clipsDescendants = true;
        fr->zIndex = zIdx;
        panel = static_cast<UIObject *>(overlay->addChild(std::move(fr)));
    }

    auto *layout = panel->addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_VERTICAL;
    layout->horizontalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;

    addItemRows(panel, items, inSubmenu, zIdx);
    panel->markDirty();
    return panel;
}

void Dropdown::addItemRows(Instance *container, std::vector<DropdownItem> *items, bool inSubmenu, int zIdx)
{
    for (size_t idx = 0; idx < items->size(); idx++) {
        DropdownItem &item = (*items)[idx];

        if (item.kind() == DropdownItem::Kind::SEPARATOR) {
            auto *sep = container->add<Frame>();
            sep->size = UDim2::fromOffset(popupWidth, 8.0f);
            sep->backgroundColor = separatorColor;
            sep->backgroundTransparency = 0.5f;
            sep->borderPixelSize = 0.0f;
            sep->interactable = false;
            sep->zIndex = zIdx;
            sep->layoutOrder = static_cast<LayoutOrder>(idx * 100);
            sep->markDirty();
            continue;
        }

        auto *row = container->add<TextButton>();
        row->size = UDim2::fromOffset(popupWidth, itemHeight);
        row->zIndex = zIdx;
        row->layoutOrder = static_cast<LayoutOrder>(idx * 100);
        row->autoButtonColor = false;
        row->backgroundColor = popupBackground;
        row->backgroundTransparency = 0.0f;
        row->borderPixelSize = 0.0f;
        row->textXAlignment = TextXAlignment::LEFT;
        row->textYAlignment = TextYAlignment::CENTER;
        row->fontSize = itemFontSize;
        row->textWrapped = false;
        row->padding = {
            UDim::fromOffset(0.0f), UDim::fromOffset(8.0f),
            UDim::fromOffset(0.0f), UDim::fromOffset(8.0f)
        };
        row->text = buildItemText(item);
        row->textColor = item.enabled ? itemTextColor : itemDisabledColor;

        if (!item.enabled) {
            row->interactable = false;
            row->markDirty();
            continue;
        }

        Color3 hoverBg = itemHoverBackground;
        Color3 normalBg = popupBackground;

        row->onMouseLeaveCb = [row, normalBg]() {
            row->backgroundColor = normalBg;
            row->markDirty();
            return EventResult::CONSUMED;
        };

        if (item.kind() == DropdownItem::Kind::SUBMENU) {
            row->onMouseEnterCb = [this, row, hoverBg, items, idx]() {
                row->backgroundColor = hoverBg;
                row->markDirty();
                auto &subItems = std::get<DropdownSubmenu>((*items)[idx].payload).items;
                float subX = m_popup->absolutePosition.x + m_popup->absoluteSize.x;
                float subY = row->absolutePosition.y;
                buildSubmenuPopup(m_overlayPtr, &subItems, {subX, subY});
                m_submenuSourceRow = row;
                return EventResult::CONSUMED;
            };
            row->onMouseLeaveCb = [this, row, normalBg, hoverBg]() {
                row->backgroundColor = (m_submenuSourceRow == row) ? hoverBg : normalBg;
                row->markDirty();
                return EventResult::CONSUMED;
            };
            row->onMouseButton1ClickCb = []() { return EventResult::CONSUMED; };
        } else {
            row->onMouseEnterCb = [this, row, hoverBg, inSubmenu]() {
                row->backgroundColor = hoverBg;
                row->markDirty();
                if (!inSubmenu) {
                    closeSubmenu();
                }
                return EventResult::CONSUMED;
            };
        }

        if (item.kind() == DropdownItem::Kind::ACTION) {
            std::function<void()> cb = std::get<DropdownAction>(item.payload).onActivate;
            row->onMouseButton1ClickCb = [this, cb]() {
                if (cb != nullptr) {
                    cb();
                }
                requestClose();
                return EventResult::CONSUMED;
            };
        } else if (item.kind() == DropdownItem::Kind::TOGGLE) {
            row->onMouseButton1ClickCb = [this, row, items, idx]() {
                auto &t = std::get<DropdownToggle>((*items)[idx].payload);
                t.toggle();
                row->text = buildItemText((*items)[idx]);
                row->markDirty();
                return EventResult::CONSUMED;
            };
        }

        row->markDirty();
    }
}

std::string Dropdown::buildItemText(const DropdownItem &item) const
{
    std::string text;
    if (item.kind() == DropdownItem::Kind::TOGGLE) {
        const auto &t = std::get<DropdownToggle>(item.payload);
        text = std::string(t.currentState() ? "\xe2\x9c\x93 " : "  ") + item.label;
    } else if (item.kind() == DropdownItem::Kind::SUBMENU) {
        text = item.label + " \xe2\x96\xb6";
    } else {
        text = item.label;
    }
    if (!item.shortcutHint.empty()) {
        text += "    " + item.shortcutHint;
    }
    return text;
}

void Dropdown::draw(DrawContext &ctx)
{
    // Safe to mutate here: Dropdown is a regular scene child drawn before
    // the OverlayLayer, so removeChild on the overlay does not invalidate
    // any in-progress overlay iteration.
    // TODO(C17): move to a proper pre-draw tick once one exists.
    if (m_state == State::PENDING_CLOSE) {
        actuallyClose();
    }
    TextButton::draw(ctx);
}

EventResult Dropdown::onMouseButton1Down(uint32_t x, uint32_t y)
{
    UIButton::onMouseButton1Down(x, y);
    if (m_state == State::CLOSED) {
        open();
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
