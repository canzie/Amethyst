#include "components/dropdown.h"

#include "amethyst/icons.h"
#include "components/image_label.h"
#include "components/text_button.h"
#include "modules/style.h"

namespace Amethyst {

Dropdown::Dropdown()
{
    setDropdownProperties({
        .popupDirection = DropdownDirection::DOWN,
        .maxVisibleItems = 8,
        .itemHeight = 24.0f,
        .popupWidth = 180.0f,
        .itemFontSize = 14.0f,
        .popupBackground = Color3{0.18f, 0.18f, 0.18f},
        .itemTextColor = Color4{0.92f, 0.92f, 0.92f, 1.0f},
        .itemDisabledColor = Color4{0.45f, 0.45f, 0.45f, 1.0f},
        .itemHoverBackground = Color3{0.25f, 0.42f, 0.65f},
        .separatorColor = Color3{0.32f, 0.32f, 0.32f},
        .indicatorSize = 10.0f,
        .indicatorPadding = 8.0f,
        .indicatorColor = Color4{0.6f, 0.6f, 0.6f, 1.0f},
    });

    m_indicator = add<ImageLabel>();
    m_indicator->setSvg(Icons::ARROW);
    m_indicator->setBaseStyleProperties({.backgroundTransparency = 1.0f, .borderPixelSize = 0.0f});

    resolveStyle();
}

void Dropdown::resolveStyle()
{
    resolveBaseStyle(ComponentType::DROPDOWN);
    TextButton::resolveStyle();

    DropdownStyleProperties resolved =
        Style::instance().getDropdownStyle(ComponentType::DROPDOWN, getClasses(), effectiveGuiState(), m_part);
    if (m_ddProps.apply(resolved)) {
        markDirty();
    }
    placeIndicator();
}

Dropdown &Dropdown::indicator(std::function<void(ImageLabel &)> fn)
{
    if (fn) {
        fn(*m_indicator);
    }
    return *this;
}

void Dropdown::placeIndicator()
{
    if (m_indicator == nullptr) {
        return;
    }

    // children lay out inside the border and padding, so the arrow is pushed out past the
    // content's right edge into the right padding, which a theme sizes to fit it
    const float rightInset = resolveContentInsets(absoluteSize).y;
    m_indicator->setBaseProperties({
        .anchorPoint = vec2(1.0f, 0.5f),
        .interactable = false,
        .position = UDim2(1.0f, rightInset - m_ddProps.indicatorPadding, 0.5f, 0.0f),
        .size = UDim2::fromOffset(m_ddProps.indicatorSize, m_ddProps.indicatorSize),
        .rotation = 90.0f,
    });
    m_indicator->setImageStyleProperties({.imageColor = m_ddProps.indicatorColor});
}

bool Dropdown::setDropdownProperties(const DropdownStylePropertiesArgs &props)
{
    bool changed = m_ddProps.apply(props);
    if (changed) {
        placeIndicator();
        if (m_contextMenu != nullptr) {
            syncContextMenu();
        }
        markDirty();
    }
    return changed;
}

void Dropdown::setItems(std::vector<std::unique_ptr<ContextMenu::ItemData>> items)
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

std::vector<std::unique_ptr<ContextMenu::ItemData>> &Dropdown::items()
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
    // hiding the menu reports the close through the menu's onClosedCb, wired up in open()
    m_contextMenu->hide();
}

void Dropdown::syncContextMenu()
{
    if (m_contextMenu == nullptr) {
        return;
    }

    ContextMenuStylePropertiesArgs cmProps;
    cmProps.itemHoverBackground = m_ddProps.itemHoverBackground;
    cmProps.separatorColor = m_ddProps.separatorColor;
    m_contextMenu->setContextMenuProperties(cmProps);

    TextStylePropertiesArgs textProps;
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
