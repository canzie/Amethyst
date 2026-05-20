#include "components/dropdown_item.h"

namespace Amethyst {

DropdownItem DropdownItem::action(std::string label, std::function<void()> cb)
{
    DropdownItem item;
    item.label = std::move(label);
    item.payload = DropdownAction{std::move(cb)};
    return item;
}

DropdownItem DropdownItem::toggle(std::string label, bool *stateRef, std::function<void(bool)> cb)
{
    DropdownItem item;
    item.label = std::move(label);
    item.payload = DropdownToggle{stateRef, false, std::move(cb)};
    return item;
}

DropdownItem DropdownItem::separator()
{
    DropdownItem item;
    item.payload = DropdownSeparator{};
    return item;
}

DropdownItem DropdownItem::submenu(std::string label, std::vector<DropdownItem> items)
{
    DropdownItem item;
    item.label = std::move(label);
    item.payload = DropdownSubmenu{std::move(items)};
    return item;
}

} // namespace Amethyst
