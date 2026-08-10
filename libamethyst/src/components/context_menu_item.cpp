#include "components/context_menu_item.h"

namespace Amethyst {

std::unique_ptr<ContextMenu::ItemData> makeActionItem(std::string label, std::function<void()> cb)
{
    auto item = std::make_unique<ContextMenuAction>();
    item->label = std::move(label);
    item->onActivate = std::move(cb);
    return item;
}

std::unique_ptr<ContextMenu::ItemData> makeToggleItem(std::string label, std::function<void(bool)> cb)
{
    auto item = std::make_unique<ContextMenuToggle>(std::move(cb));
    item->label = std::move(label);
    return item;
}

std::unique_ptr<ContextMenu::ItemData> makeSeparatorItem()
{
    return std::make_unique<ContextMenuSeparator>();
}

std::unique_ptr<ContextMenu::ItemData> makeSubmenuItem(std::string label, std::vector<std::unique_ptr<ContextMenu::ItemData>> items,
                                                       std::function<void()> onActivate)
{
    auto item = std::make_unique<ContextMenuSubmenu>();
    item->label = std::move(label);
    item->items = std::move(items);
    item->onActivate = std::move(onActivate);
    return item;
}

std::unique_ptr<ContextMenu::ItemData> makeRadioItem(std::string label, RadioGroup *group, int32_t value)
{
    auto item = std::make_unique<ContextMenuRadio>();
    item->label = std::move(label);
    item->group = group;
    item->value = value;
    return item;
}

} // namespace Amethyst
