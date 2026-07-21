#include "components/dropdown.h"

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
        if (m_contextMenu != nullptr) {
            syncContextMenu();
        }
        markDirty();
    }
    return changed;
}

void Dropdown::setItems(std::vector<ContextMenuItem> items)
{
    if (isOpen()) {
        close();
    }
    if (m_contextMenu != nullptr) {
        m_contextMenu->setItems(std::move(items));
    } else {
        m_items = std::move(items);
    }
}

std::vector<ContextMenuItem> &Dropdown::items()
{
    if (m_contextMenu != nullptr) {
        return m_contextMenu->items();
    }
    return m_items;
}

bool Dropdown::isOpen() const
{
    return m_contextMenu != nullptr && m_contextMenu->isOpen();
}

void Dropdown::open()
{
    if (isOpen()) {
        return;
    }

    if (m_contextMenu == nullptr) {
        m_contextMenu = add<ContextMenu>();
    }

    if (!m_items.empty()) {
        m_contextMenu->setItems(std::move(m_items));
        m_items.clear();
    }
    syncContextMenu();

    m_contextMenu->show(this);

    if (!m_contextMenu->onClosedCb) {
        m_contextMenu->onClosedCb = [this]() {
            if (onClosedCb) {
                onClosedCb();
            }
        };
    }

    if (onOpenedCb) {
        onOpenedCb();
    }
}

void Dropdown::close()
{
    if (!isOpen()) {
        return;
    }
    m_contextMenu->hide();
    if (onClosedCb) {
        onClosedCb();
    }
}

void Dropdown::syncContextMenu()
{
    if (m_contextMenu == nullptr) {
        return;
    }

    ContextMenuStyleProperties cmProps;
    cmProps.itemHoverBackground = m_ddProps.itemHoverBackground;
    cmProps.separatorColor = m_ddProps.separatorColor;
    m_contextMenu->setContextMenuProperties(cmProps);

    TextStyleProperties textProps;
    textProps.fontSize = m_ddProps.itemFontSize;
    textProps.textColor = m_ddProps.itemTextColor;
    textProps.textXAlignment = TextXAlignment::LEFT;
    textProps.textYAlignment = TextYAlignment::CENTER;
    textProps.textWrapped = false;
    m_contextMenu->setTextStyleProperties(textProps);

    m_contextMenu->maxVisibleItems = m_ddProps.maxVisibleItems;
    m_contextMenu->itemHeight = m_ddProps.itemHeight;
    m_contextMenu->popupWidth = m_ddProps.popupWidth;

    m_contextMenu->setBaseStyleProperties({
        .backgroundColor = m_ddProps.popupBackground,
        .backgroundTransparency = 0.0f,
        .borderPixelSize = 0.0f,
    });

    switch (m_ddProps.popupDirection) {
    case DropdownDirection::UP:
        m_contextMenu->placement = PopupPlacement::ABOVE;
        break;
    case DropdownDirection::LEFT:
        m_contextMenu->placement = PopupPlacement::LEFT;
        break;
    case DropdownDirection::RIGHT:
        m_contextMenu->placement = PopupPlacement::RIGHT;
        break;
    default:
        m_contextMenu->placement = PopupPlacement::BELOW;
        break;
    }
}

EventResult Dropdown::onMouseButton1Down(int32_t x, int32_t y)
{
    UIButton::onMouseButton1Down(x, y);
    if (!isOpen()) {
        open();
    }
    return EventResult::CONSUMED;
}

} // namespace Amethyst
