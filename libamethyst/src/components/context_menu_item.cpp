#include "components/context_menu_item.h"

namespace Amethyst {

ContextMenuItem ContextMenuItem::action(std::string label, std::function<void()> cb)
{
    ContextMenuItem item;
    item.label = std::move(label);
    item.payload = ContextMenuAction{std::move(cb)};
    return item;
}

ContextMenuItem ContextMenuItem::toggle(std::string label, std::function<void(bool)> cb)
{
    ContextMenuItem item;
    item.label = std::move(label);
    item.payload = ContextMenuToggle(std::move(cb));
    return item;
}

ContextMenuItem ContextMenuItem::separator()
{
    ContextMenuItem item;
    item.payload = ContextMenuSeparator{};
    return item;
}

ContextMenuItem ContextMenuItem::submenu(std::string label, std::vector<ContextMenuItem> items)
{
    ContextMenuItem item;
    item.label = std::move(label);
    item.payload = ContextMenuSubmenu{std::move(items)};
    return item;
}

} // namespace Amethyst
