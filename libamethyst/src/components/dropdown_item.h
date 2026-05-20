/*
 * Data types for Dropdown menu items
 */

#ifndef AMETHYST__DROPDOWN_ITEM_H
#define AMETHYST__DROPDOWN_ITEM_H

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace Amethyst {

class DropdownItem;

struct DropdownAction {
    std::function<void()> onActivate;
};

struct DropdownToggle {
    /// stateRef must outlive the DropdownItem if provided.
    bool *stateRef = nullptr;
    bool value = false;
    std::function<void(bool)> onToggled;

    bool currentState() const { return stateRef ? *stateRef : value; }
    void toggle()
    {
        bool next = !currentState();
        if (stateRef)
            *stateRef = next;
        else
            value = next;
        if (onToggled) onToggled(next);
    }
};

struct DropdownSeparator {};

struct DropdownSubmenu {
    std::vector<DropdownItem> items;
};

class DropdownItem {
  public:
    using Payload = std::variant<DropdownAction, DropdownToggle, DropdownSeparator, DropdownSubmenu>;

    enum class Kind { ACTION, TOGGLE, SEPARATOR, SUBMENU };

    std::string label;
    std::string shortcutHint;
    bool enabled = true;
    Payload payload;

    Kind kind() const { return static_cast<Kind>(payload.index()); }

    static DropdownItem action(std::string label, std::function<void()> cb);
    static DropdownItem toggle(std::string label, bool *stateRef = nullptr,
                               std::function<void(bool)> cb = {});
    static DropdownItem separator();
    static DropdownItem submenu(std::string label, std::vector<DropdownItem> items);

    DropdownItem &withShortcut(std::string hint)
    {
        shortcutHint = std::move(hint);
        return *this;
    }
    DropdownItem &withEnabled(bool e)
    {
        enabled = e;
        return *this;
    }
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_ITEM_H
