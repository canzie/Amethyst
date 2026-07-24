/*
 * Data types for context menu items (dropdowns, menu bars, right-click menus).
 */

#ifndef AMETHYST__CONTEXT_MENU_ITEM_H
#define AMETHYST__CONTEXT_MENU_ITEM_H

#include <functional>
#include <string>
#include <variant>
#include <vector>

namespace Amethyst {

class ContextMenuItem;

struct ContextMenuAction {
    std::function<void()> onActivate;
};

struct ContextMenuToggle {
    explicit ContextMenuToggle(std::function<void(bool)> cb) : onToggled(std::move(cb)) {}

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

struct ContextMenuSeparator {};
struct ContextMenuSelect {};

struct ContextMenuSubmenu {
    std::vector<ContextMenuItem> items;
};

class ContextMenuItem {
  public:
    using Payload = std::variant<ContextMenuAction, ContextMenuToggle, ContextMenuSeparator, ContextMenuSubmenu, ContextMenuSelect>;

    enum class Kind {
        ACTION,    // simple fire cb on click
        TOGGLE,    // checkbox
        SEPARATOR, // duh
        SUBMENU,   // pure submenu entry
        SELECT,    // ???
        RADIO      // radio button style
    };

    std::string label;
    std::string shortcutHint;
    bool enabled = true;
    Payload payload;

    Kind kind() const { return static_cast<Kind>(payload.index()); }

    static ContextMenuItem action(std::string label, std::function<void()> cb);
    static ContextMenuItem toggle(std::string label, std::function<void(bool)> cb = {});
    static ContextMenuItem separator();
    static ContextMenuItem submenu(std::string label, std::vector<ContextMenuItem> items);

    ContextMenuItem &withShortcut(std::string hint)
    {
        shortcutHint = std::move(hint);
        return *this;
    }
    ContextMenuItem &withEnabled(bool e)
    {
        enabled = e;
        return *this;
    }
};

using DropdownItem = ContextMenuItem;
using DropdownAction = ContextMenuAction;
using DropdownToggle = ContextMenuToggle;
using DropdownSeparator = ContextMenuSeparator;
using DropdownSubmenu = ContextMenuSubmenu;
using DropdownSelect = ContextMenuSelect;

} // namespace Amethyst

#endif // AMETHYST__CONTEXT_MENU_ITEM_H
