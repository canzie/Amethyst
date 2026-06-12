#include "components/menu_bar.h"

#include "components/extensions/ui_list_layout.h"

#include <climits>

namespace Amethyst {

MenuBar::MenuBar()
{
    m_mbProps.entryPaddingX = 12.0f;
    m_mbProps.entryPaddingY = 4.0f;
    m_mbProps.entryFontSize = 14.0f;
    m_mbProps.entryHoverBackground = Color3{0.25f, 0.25f, 0.30f};
    m_mbProps.entryActiveBackground = Color3{0.28f, 0.42f, 0.62f};

    auto *layout = addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_HORIZONTAL;
    layout->verticalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;
}

Dropdown *MenuBar::addMenu(std::string label, std::vector<DropdownItem> items)
{
    auto *entry = add<Dropdown>();
    entry->setItems(std::move(items));
    entry->setDropdownProperties({
        .popupDirection = DropdownDirection::DOWN,
        .maxVisibleItems = INT_MAX,
    });
    entry->setTextStyleProperties({
        .fontSize = m_mbProps.entryFontSize,
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
    });
    entry->setText(label);
    entry->setButtonProperties({.autoButtonColor = false});
    float estWidth = static_cast<float>(label.size()) * m_mbProps.entryFontSize * 0.6f + 2.0f * m_mbProps.entryPaddingX;
    entry->setBaseStyleProperties({
        .backgroundColor = getBaseStyleProperties().backgroundColor,
        .backgroundTransparency = getBaseStyleProperties().backgroundTransparency,
        .borderPixelSize = 0.0f,
    });
    entry->setBaseProperties({
        .layoutOrder = static_cast<LayoutOrder>(m_entries.size()),
        .padding = {UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX),
                    UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX)},
        .size = UDim2::fromOffset(estWidth, 0.0f),
    });

    entry->onMouseEnterCb = [this, entry]() {
        Color3 newBg = entry->isOpen() ? m_mbProps.entryActiveBackground : m_mbProps.entryHoverBackground;
        if (entry->getBaseStyleProperties().backgroundColor != newBg) {
            entry->setBaseStyleProperties({.backgroundColor = newBg});
        }
        onEntryHovered(entry);
        return EventResult::CONSUMED;
    };
    entry->onMouseLeaveCb = [this, entry]() {
        Color3 newBg = entry->isOpen() ? m_mbProps.entryActiveBackground : getBaseStyleProperties().backgroundColor;
        if (entry->getBaseStyleProperties().backgroundColor != newBg) {
            entry->setBaseStyleProperties({.backgroundColor = newBg});
        }
        return EventResult::CONSUMED;
    };
    entry->onOpenedCb = [this, entry]() {
        m_openEntry = entry;
        if (entry->getBaseStyleProperties().backgroundColor != m_mbProps.entryActiveBackground) {
            entry->setBaseStyleProperties({.backgroundColor = m_mbProps.entryActiveBackground});
        }
    };
    entry->onClosedCb = [this, entry]() {
        onEntryClosed(entry);
        if (entry->getBaseStyleProperties().backgroundColor != getBaseStyleProperties().backgroundColor) {
            entry->setBaseStyleProperties({.backgroundColor = getBaseStyleProperties().backgroundColor});
        }
    };

    m_entries.push_back(entry);
    markDirty();
    return entry;
}

void MenuBar::clear()
{
    for (auto *e : m_entries) {
        if (e->isOpen()) e->requestClose();
    }
    removeAllChildren();
    m_entries.clear();
    m_openEntry = nullptr;
    markDirty();
}

void MenuBar::onEntryHovered(Dropdown *entry)
{
    if (m_openEntry && m_openEntry != entry) {
        m_openEntry->closeImmediate();
        entry->open();
    }
}

void MenuBar::onEntryClosed(Dropdown *entry)
{
    if (m_openEntry == entry) {
        m_openEntry = nullptr;
    }
}

bool MenuBar::setMenuBarProperties(const MenuBarStyleProperties &props)
{
    bool changed = m_mbProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

} // namespace Amethyst
