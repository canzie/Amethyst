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
        .maxVisibleItems = INT_MAX,
        .popupDirection = DropdownDirection::DOWN,
    });
    entry->setTextProperties({
        .text = label,
        .fontSize = m_mbProps.entryFontSize,
        .textXAlignment = TextXAlignment::CENTER,
        .textYAlignment = TextYAlignment::CENTER,
    });
    entry->setButtonProperties({.autoButtonColor = 0});
    entry->setBaseProperties({
        .backgroundColor = getBaseProperties().backgroundColor,
        .backgroundTransparency = getBaseProperties().backgroundTransparency,
        .borderPixelSize = 0.0f,
    });
    entry->setBaseProperties({
        .padding = {
            UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX),
            UDim::fromOffset(m_mbProps.entryPaddingY), UDim::fromOffset(m_mbProps.entryPaddingX)
        },
    });
    float estWidth = static_cast<float>(label.size()) * m_mbProps.entryFontSize * 0.6f + 2.0f * m_mbProps.entryPaddingX;
    entry->setBaseProperties({.size = UDim2::fromOffset(estWidth, 0.0f)});
    entry->layoutOrder = static_cast<LayoutOrder>(m_entries.size());

    entry->onMouseEnterCb = [this, entry]() {
        Color3 newBg = entry->isOpen() ? m_mbProps.entryActiveBackground : m_mbProps.entryHoverBackground;
        if (entry->getBaseProperties().backgroundColor != newBg) {
            entry->setBaseProperties({.backgroundColor = newBg});
        }
        onEntryHovered(entry);
        return EventResult::CONSUMED;
    };
    entry->onMouseLeaveCb = [this, entry]() {
        Color3 newBg = entry->isOpen() ? m_mbProps.entryActiveBackground : getBaseProperties().backgroundColor;
        if (entry->getBaseProperties().backgroundColor != newBg) {
            entry->setBaseProperties({.backgroundColor = newBg});
        }
        return EventResult::CONSUMED;
    };
    entry->onOpenedCb = [this, entry]() {
        m_openEntry = entry;
        if (entry->getBaseProperties().backgroundColor != m_mbProps.entryActiveBackground) {
            entry->setBaseProperties({.backgroundColor = m_mbProps.entryActiveBackground});
        }
    };
    entry->onClosedCb = [this, entry]() {
        onEntryClosed(entry);
        if (entry->getBaseProperties().backgroundColor != getBaseProperties().backgroundColor) {
            entry->setBaseProperties({.backgroundColor = getBaseProperties().backgroundColor});
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

bool MenuBar::setMenuBarProperties(const MenuBarProperties &props)
{
    bool changed = false;
#define AM_APPLY(field) \
    if (propIsSet(props.field) && m_mbProps.field != props.field) { \
        m_mbProps.field = props.field; \
        changed = true; \
    }
    AM_APPLY(entryPaddingX)
    AM_APPLY(entryPaddingY)
    AM_APPLY(entryFontSize)
    AM_APPLY(entryHoverBackground)
    AM_APPLY(entryActiveBackground)
#undef AM_APPLY
    if (changed) {
        markDirty();
    }
    return changed;
}

} // namespace Amethyst
