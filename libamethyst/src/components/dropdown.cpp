#include "components/dropdown.h"

#include "components/dropdown_item.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/overlay_layer.h"
#include "components/scrolling_frame.h"
#include "components/window.h"
#include "rendering/draw_context.h"

#include <algorithm>
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
    for (auto *panel : m_submenuStack) {
        panel->visible = false;
        panel->markDirty();
    }
    if (m_eater) {
        m_eater->visible = false;
        m_eater->interactable = false;
    }
    markDirty();
}

void Dropdown::closeImmediate()
{
    if (m_state == State::CLOSED) {
        return;
    }
    actuallyClose();
}

void Dropdown::actuallyClose()
{
    if (m_overlayPtr) {
        for (auto *panel : m_submenuStack) {
            m_overlayPtr->removeChild(panel);
        }
        if (m_popup) {
            m_overlayPtr->removeChild(m_popup);
        }
        if (m_eater) {
            m_overlayPtr->removeChild(m_eater);
        }
        m_overlayPtr = nullptr;
    }
    m_submenuStack.clear();
    m_submenuSourceRows.clear();
    m_popup = nullptr;
    m_eater = nullptr;
    m_state = State::CLOSED;

    if (onClosedCb) {
        onClosedCb();
    }
}

void Dropdown::closeSubmenuFrom(size_t depth)
{
    for (size_t i = depth; i < m_submenuSourceRows.size(); i++) {
        if (m_submenuSourceRows[i]) {
            m_submenuSourceRows[i]->backgroundColor = popupBackground;
            m_submenuSourceRows[i]->markDirty();
        }
    }
    m_submenuSourceRows.resize(depth);

    if (m_overlayPtr) {
        for (size_t i = depth; i < m_submenuStack.size(); i++) {
            m_overlayPtr->removeChild(m_submenuStack[i]);
        }
    }
    m_submenuStack.resize(depth);
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

    glm::vec2 viewport = m_overlayPtr->absoluteSize;
    pos.x = std::min(pos.x, std::max(0.0f, viewport.x - popupWidth));
    pos.y = std::min(pos.y, std::max(0.0f, viewport.y - visibleHeight));

    m_popup = buildPopupPanel(overlay, pos, totalHeight, visibleHeight, 1, {});
}

void Dropdown::buildSubmenuAtPath(OverlayLayer *overlay, std::vector<size_t> path, glm::vec2 pos)
{
    size_t depth = path.size() - 1;
    closeSubmenuFrom(depth);

    auto &subItems = itemsAtPath(path);
    float totalHeight = computeTotalHeight(subItems);
    float visibleHeight = (maxVisibleItems == INT_MAX)
        ? totalHeight
        : std::min(totalHeight, static_cast<float>(maxVisibleItems) * itemHeight);

    glm::vec2 viewport = overlay->absoluteSize;
    pos.y = std::min(pos.y, std::max(0.0f, viewport.y - visibleHeight));

    UIObject *panel = buildPopupPanel(overlay, pos, totalHeight, visibleHeight,
                                      2 + static_cast<int>(depth), path);
    m_submenuStack.push_back(panel);
}

std::vector<DropdownItem> &Dropdown::itemsAtPath(const std::vector<size_t> &path)
{
    std::vector<DropdownItem> *items = &m_items;
    for (size_t idx : path) {
        items = &std::get<DropdownSubmenu>((*items)[idx].payload).items;
    }
    return *items;
}

UIObject *Dropdown::buildPopupPanel(OverlayLayer *overlay, glm::vec2 pos, float totalHeight,
                                     float visibleHeight, int zIdx, std::vector<size_t> path)
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
        sf->clipsDescendants = true;
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

    addItemRows(panel, zIdx + 1, std::move(path));
    panel->markDirty();
    return panel;
}

void Dropdown::addItemRows(Instance *container, int zIdx, std::vector<size_t> path)
{
    std::vector<DropdownItem> &items = itemsAtPath(path);
    size_t depth = path.size();

    for (size_t idx = 0; idx < items.size(); idx++) {
        DropdownItem &item = items[idx];

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

        row->onMouseLeaveCb = [this, row, normalBg, hoverBg, depth]() {
            bool isSubmenuSource = depth < m_submenuSourceRows.size() &&
                                   m_submenuSourceRows[depth] == row;
            row->backgroundColor = isSubmenuSource ? hoverBg : normalBg;
            row->markDirty();
            return EventResult::CONSUMED;
        };

        if (item.kind() == DropdownItem::Kind::SUBMENU) {
            row->onMouseEnterCb = [this, row, hoverBg, path, idx]() {
                row->backgroundColor = hoverBg;
                row->markDirty();
                UIObject *parentPanel = path.empty() ? m_popup : m_submenuStack[path.size() - 1];
                float preferredX = parentPanel->absolutePosition.x + parentPanel->absoluteSize.x;
                float altX = parentPanel->absolutePosition.x - popupWidth;
                float viewportW = m_overlayPtr->absoluteSize.x;
                float subX = (preferredX + popupWidth <= viewportW) ? preferredX
                           : (altX >= 0.0f)                         ? altX
                           : std::max(0.0f, viewportW - popupWidth);
                float subY = row->absolutePosition.y;
                std::vector<size_t> subPath = path;
                subPath.push_back(idx);
                buildSubmenuAtPath(m_overlayPtr, std::move(subPath), {subX, subY});
                m_submenuSourceRows.push_back(row);
                return EventResult::CONSUMED;
            };
            row->onMouseButton1ClickCb = []() { return EventResult::CONSUMED; };
        } else {
            row->onMouseEnterCb = [this, row, hoverBg, depth]() {
                row->backgroundColor = hoverBg;
                row->markDirty();
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
        } else if (item.kind() == DropdownItem::Kind::TOGGLE) {
            row->onMouseButton1ClickCb = [this, row, path, idx]() {
                auto &t = std::get<DropdownToggle>(itemsAtPath(path)[idx].payload);
                t.toggle();
                row->text = buildItemText(itemsAtPath(path)[idx]);
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
