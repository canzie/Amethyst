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
    explicit DropdownToggle(std::function<void(bool)> cb) : onToggled(std::move(cb)) {}

    std::function<void(bool)> onToggled;

    bool currentState() const { return m_value; }
    void toggle()
    {
        m_value = !m_value;
        if (onToggled) onToggled(m_value);
    }

  private:
    bool m_value = false;
};

struct DropdownSeparator {};
struct DropdownSelect {};

struct DropdownSubmenu {
    std::vector<DropdownItem> items;
};

class DropdownItem {
  public:
    using Payload = std::variant<DropdownAction, DropdownToggle, DropdownSeparator, DropdownSubmenu, DropdownSelect>;

    enum class Kind {
        ACTION,
        TOGGLE,
        SEPARATOR,
        SUBMENU,
        SELECT
    };

    std::string label;
    std::string shortcutHint;
    bool enabled = true;
    Payload payload;

    Kind kind() const { return static_cast<Kind>(payload.index()); }

    static DropdownItem action(std::string label, std::function<void()> cb);
    static DropdownItem toggle(std::string label, std::function<void(bool)> cb = {});
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
