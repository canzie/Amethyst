#include "components/menu_bar.h"

#include "components/extensions/ui_list_layout.h"
#include "modules/style.h"

#include <climits>

namespace Amethyst {

MenuBar::MenuBar()
{
    m_mbProps.entryPaddingX = 12.0f;
    m_mbProps.entryPaddingY = 4.0f;
    m_mbProps.entryFontSize = 14.0f;

    auto *layout = addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_HORIZONTAL;
    layout->verticalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;

    resolveStyle();
}

void MenuBar::resolveStyle()
{
    resolveBaseStyle(ComponentType::MENU_BAR);

    MenuBarStyleProperties resolved = Style::instance().getMenuBarStyle(ComponentType::MENU_BAR, getClasses(), effectiveGuiState());
    if (m_mbProps.apply(resolved)) {
        markDirty();
    }

    propagateClassesToEntries();
}

void MenuBar::propagateClassesToEntries()
{
    for (auto *entry : m_entries) {
        entry->setClasses(getClasses());
    }
}

Dropdown *MenuBar::addMenu(std::string label, std::vector<std::unique_ptr<ContextMenu::ItemData>> items)
{
    auto *entry = add<Dropdown>();
    entry->setItems(std::move(items));
    entry->setDropdownProperties({
        .popupDirection = DropdownDirection::DOWN,
        .maxVisibleItems = INT_MAX,
    });
    entry->setTextStyleProperties({
        .fontSize = m_mbProps.entryFontSize,
        .textColor = Style::instance().getTextStyle(ComponentType::MENU_BAR, getClasses()).textColor,
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
    });
    entry->setText(label);
    entry->setButtonProperties({.autoButtonColor = false});
    entry->bindPart(ComponentPart::ENTRY);
    entry->setClasses(getClasses());
    float estWidth = static_cast<float>(label.size()) * m_mbProps.entryFontSize * 0.6f + 2.0f * m_mbProps.entryPaddingX;
    entry->setBaseProperties({
        .layoutOrder = static_cast<LayoutOrder>(m_entries.size()),
        .padding = {UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX),
                    UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX)},
        .size = UDim2::fromOffset(estWidth, 0.0f),
    });

    entry->onMouseEnterCb = [this, entry]() {
        entry->setGuiState(static_cast<uint16_t>(entry->getGuiState() | GUI_STATE_HOVERED));
        onEntryHovered(entry);
        return EventResult::CONSUMED;
    };
    entry->onMouseLeaveCb = [entry]() {
        entry->setGuiState(static_cast<uint16_t>(entry->getGuiState() & ~GUI_STATE_HOVERED));
        return EventResult::CONSUMED;
    };
    entry->onOpenedCb = [this, entry]() {
        m_openEntry = entry;
        entry->setGuiState(static_cast<uint16_t>(entry->getGuiState() | GUI_STATE_ACTIVE));
    };
    entry->onClosedCb = [this, entry]() {
        onEntryClosed(entry);
        entry->setGuiState(static_cast<uint16_t>(entry->getGuiState() & ~GUI_STATE_ACTIVE));
    };

    m_entries.push_back(entry);
    markDirty();
    return entry;
}

void MenuBar::clear()
{
    for (auto *e : m_entries) {
        if (e->isOpen()) e->close();
    }
    removeAllChildren();
    m_entries.clear();
    m_openEntry = nullptr;
    markDirty();
}

void MenuBar::onEntryHovered(Dropdown *entry)
{
    if (m_openEntry && m_openEntry != entry) {
        m_openEntry->close();
        entry->open();
    }
}

void MenuBar::onEntryClosed(Dropdown *entry)
{
    if (m_openEntry == entry) {
        m_openEntry = nullptr;
    }
}

bool MenuBar::setMenuBarProperties(const MenuBarStylePropertiesArgs &props)
{
    bool changed = m_mbProps.apply(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

} // namespace Amethyst
