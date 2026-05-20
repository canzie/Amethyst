#include "components/menu_bar.h"

#include "components/extensions/ui_list_layout.h"
#include "rendering/draw_context.h"

#include <climits>

namespace Amethyst {

MenuBar::MenuBar()
{
    auto *layout = addExtension<UIListLayout>();
    layout->fillDirection = FillDirection::FILL_HORIZONTAL;
    layout->verticalFlex = UiFlexAlignment::FILL;
    layout->innerPadding = UDim::fromOffset(0.0f);
    layout->sortOrder = SortOrder::SORT_LAYOUT_ORDER;
}

Dropdown *MenuBar::addMenu(std::string label, std::vector<DropdownItem> items)
{
    auto *entry = add<Dropdown>();
    entry->text = label;
    entry->setItems(std::move(items));
    entry->maxVisibleItems = INT_MAX;
    entry->popupDirection = DropdownDirection::DOWN;
    entry->fontSize = entryFontSize;
    entry->autoButtonColor = false;
    entry->backgroundColor = backgroundColor;
    entry->backgroundTransparency = backgroundTransparency;
    entry->borderPixelSize = 0.0f;
    entry->textXAlignment = TextXAlignment::CENTER;
    entry->textYAlignment = TextYAlignment::CENTER;
    entry->padding = {
        UDim::fromOffset(entryPaddingY), UDim::fromOffset(entryPaddingX),
        UDim::fromOffset(entryPaddingY), UDim::fromOffset(entryPaddingX)
    };
    // estimate width from label; height filled by UIListLayout verticalFlex
    float estWidth = static_cast<float>(label.size()) * entryFontSize * 0.6f + 2.0f * entryPaddingX;
    entry->size = UDim2::fromOffset(estWidth, 0.0f);
    entry->layoutOrder = static_cast<LayoutOrder>(m_entries.size());

    Color3 hoverBg = entryHoverBackground;
    Color3 activeBg = entryActiveBackground;
    Color3 normalBg = backgroundColor;

    entry->onMouseEnterCb = [this, entry, hoverBg, activeBg]() {
        entry->backgroundColor = entry->isOpen() ? activeBg : hoverBg;
        entry->markDirty();
        onEntryHovered(entry);
        return EventResult::CONSUMED;
    };
    entry->onMouseLeaveCb = [entry, normalBg, activeBg]() {
        entry->backgroundColor = entry->isOpen() ? activeBg : normalBg;
        entry->markDirty();
        return EventResult::CONSUMED;
    };
    entry->onOpenedCb = [this, entry, activeBg]() {
        m_openEntry = entry;
        entry->backgroundColor = activeBg;
        entry->markDirty();
    };
    entry->onClosedCb = [this, entry, normalBg]() {
        onEntryClosed(entry);
        entry->backgroundColor = normalBg;
        entry->markDirty();
    };

    entry->markDirty();
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
        m_openEntry->requestClose();
        entry->open();
    }
}

void MenuBar::onEntryClosed(Dropdown *entry)
{
    if (m_openEntry == entry) {
        m_openEntry = nullptr;
    }
}

void MenuBar::draw(DrawContext &ctx)
{
    Frame::draw(ctx);
}

} // namespace Amethyst
