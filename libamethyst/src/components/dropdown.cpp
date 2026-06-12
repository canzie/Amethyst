#include "components/dropdown.h"

#include "components/dropdown_item.h"
#include "components/extensions/ui_list_layout.h"
#include "components/frame.h"
#include "components/invisible_button.h"
#include "components/overlay_layer.h"
#include "components/scrolling_frame.h"
#include "components/window.h"
#include "rendering/draw_context.h"

#include "math/math.h"
#include <algorithm>
#include <climits>

namespace Amethyst {

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
    if (win == nullptr) {
        return;
    }

    m_overlayPtr = win->getOverlayLayer();

    auto eater = std::make_unique<InvisibleButton>();
    eater->setBaseProperties({.size = UDim2::fromScale(1.0f, 1.0f), .zIndex = 0});
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
        m_popup->setBaseProperties({.visible = 0});
    }
    for (auto *panel : m_submenuStack) {
        panel->setBaseProperties({.visible = 0});
    }
    if (m_eater) {
        m_eater->setBaseProperties({.interactable = 0, .visible = 0});
    }
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
            m_submenuSourceRows[i]->setBaseStyleProperties({.backgroundColor = m_ddProps.popupBackground});
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
        h += (item.kind() == DropdownItem::Kind::SEPARATOR) ? 8.0f : m_ddProps.itemHeight;
    }
    return h;
}

void Dropdown::buildMainPopup(OverlayLayer *overlay)
{
    float totalHeight = computeTotalHeight(m_items);
    float visibleHeight = (m_ddProps.maxVisibleItems == INT_MAX)
                              ? totalHeight
                              : std::min(totalHeight, static_cast<float>(m_ddProps.maxVisibleItems) * m_ddProps.itemHeight);

    vec2 pos;
    switch (m_ddProps.popupDirection) {
    case DropdownDirection::UP:
        pos = {absolutePosition.x, absolutePosition.y - visibleHeight};
        break;
    case DropdownDirection::LEFT:
        pos = {absolutePosition.x - m_ddProps.popupWidth, absolutePosition.y};
        break;
    case DropdownDirection::RIGHT:
        pos = {absolutePosition.x + absoluteSize.x, absolutePosition.y};
        break;
    default:
        pos = {absolutePosition.x, absolutePosition.y + absoluteSize.y};
        break;
    }

    vec2 viewport = m_overlayPtr->absoluteSize;
    pos.x = std::min(pos.x, std::max(0.0f, viewport.x - m_ddProps.popupWidth));
    pos.y = std::min(pos.y, std::max(0.0f, viewport.y - visibleHeight));

    m_popup = buildPopupPanel(overlay, pos, totalHeight, visibleHeight, 1, {});
}

void Dropdown::buildSubmenuAtPath(OverlayLayer *overlay, const std::vector<size_t> &path, vec2 pos)
{
    size_t depth = path.size() - 1;
    closeSubmenuFrom(depth);

    auto &subItems = itemsAtPath(path);
    float totalHeight = computeTotalHeight(subItems);
    float visibleHeight = (m_ddProps.maxVisibleItems == INT_MAX)
                              ? totalHeight
                              : std::min(totalHeight, static_cast<float>(m_ddProps.maxVisibleItems) * m_ddProps.itemHeight);

    vec2 viewport = overlay->absoluteSize;
    pos.y = std::min(pos.y, std::max(0.0f, viewport.y - visibleHeight));

    UIObject *panel = buildPopupPanel(overlay, pos, totalHeight, visibleHeight, 2 + static_cast<int>(depth), path);
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

UIObject *Dropdown::buildPopupPanel(OverlayLayer *overlay, vec2 pos, float totalHeight, float visibleHeight, int zIdx,
                                    const std::vector<size_t> &path)
{
    UIObject *panel;

    if (totalHeight > visibleHeight + 0.5f) {
        auto sf = std::make_unique<ScrollingFrame>();
        sf->setBaseStyleProperties({
            .backgroundColor = m_ddProps.popupBackground,
            .backgroundTransparency = 0.0f,
            .borderPixelSize = 0.0f,
        });
        sf->setBaseProperties({
            .clipsDescendants = true,
            .position = UDim2::fromOffset(pos.x, pos.y),
            .size = UDim2::fromOffset(m_ddProps.popupWidth, visibleHeight),
            .zIndex = zIdx,
        });
        sf->setScrollingFrameProperties({
            .scrollAxis = ScrollAxis::Y,
            .scrollBarVisibility = ScrollBarVisibility::AUTO,
            .canvasSize = UDim2::fromOffset(m_ddProps.popupWidth, totalHeight),
        });
        panel = static_cast<UIObject *>(overlay->addChild(std::move(sf)));
    } else {
        auto fr = std::make_unique<Frame>();
        fr->setBaseStyleProperties({
            .backgroundColor = m_ddProps.popupBackground,
            .backgroundTransparency = 0.0f,
            .borderPixelSize = 0.0f,
        });
        fr->setBaseProperties({
            .clipsDescendants = true,
            .position = UDim2::fromOffset(pos.x, pos.y),
            .size = UDim2::fromOffset(m_ddProps.popupWidth, visibleHeight),
            .zIndex = zIdx,
        });
        panel = static_cast<UIObject *>(overlay->addChild(std::move(fr)));
    }

    auto *layout = panel->addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_VERTICAL;
    layout->horizontalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;

    addItemRows(panel, zIdx + 1, path);
    panel->markDirty();
    return panel;
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
            size_t parentDepth = path.size();
            row->onMouseEnterCb = [this, row, hoverBg, fullPath = std::move(fullPath), parentDepth]() {
                if (row->getBaseStyleProperties().backgroundColor != hoverBg) {
                    row->setBaseStyleProperties({.backgroundColor = hoverBg});
                }
                UIObject *parentPanel = parentDepth == 0 ? m_popup : m_submenuStack[parentDepth - 1];
                float preferredX = parentPanel->absolutePosition.x + parentPanel->absoluteSize.x;
                float altX = parentPanel->absolutePosition.x - m_ddProps.popupWidth;
                float viewportW = m_overlayPtr->absoluteSize.x;
                float subX = (preferredX + m_ddProps.popupWidth <= viewportW) ? preferredX
                             : (altX >= 0.0f)                                 ? altX
                                                                              : std::max(0.0f, viewportW - m_ddProps.popupWidth);
                float subY = row->absolutePosition.y;
                buildSubmenuAtPath(m_overlayPtr, fullPath, {subX, subY});
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
