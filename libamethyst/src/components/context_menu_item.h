/*
 * Amethyst's own default item data types - each is just ContextMenu's per-kind
 * ItemData base plus presentation fields (label, shortcut, enabled). Apps that
 * want custom item data subclass ContextMenu::ActionItemData etc. directly
 * instead of these.
 */

#ifndef AMETHYST__CONTEXT_MENU_ITEM_H
#define AMETHYST__CONTEXT_MENU_ITEM_H

#include "components/context_menu.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Amethyst {

class ContextMenuAction : public ContextMenu::ActionItemData {
  public:
    std::string label;
    std::string shortcutHint;
    bool enabled = true;

    std::function<void(UIObject &row)> content; // empty = default text-row builder
};

class ContextMenuToggle : public ContextMenu::ToggleItemData {
  public:
    explicit ContextMenuToggle(std::function<void(bool)> cb = {}) : ToggleItemData(std::move(cb)) {}

    std::string label;
    std::string shortcutHint;
    bool enabled = true;
};

class ContextMenuSeparator : public ContextMenu::SeparatorItemData {};

class ContextMenuSubmenu : public ContextMenu::SubmenuItemData {
  public:
    std::string label;
    bool enabled = true;
};

class ContextMenuRadio : public ContextMenu::RadioItemData {
  public:
    std::string label;
    std::string shortcutHint;
    bool enabled = true;
};

std::unique_ptr<ContextMenu::ItemData> makeActionItem(std::string label, std::function<void()> cb);
std::unique_ptr<ContextMenu::ItemData> makeToggleItem(std::string label, std::function<void(bool)> cb = {});
std::unique_ptr<ContextMenu::ItemData> makeSeparatorItem();
std::unique_ptr<ContextMenu::ItemData> makeSubmenuItem(std::string label, std::vector<std::unique_ptr<ContextMenu::ItemData>> items);
std::unique_ptr<ContextMenu::ItemData> makeRadioItem(std::string label, RadioGroup *group, int32_t value);

} // namespace Amethyst

#endif // AMETHYST__CONTEXT_MENU_ITEM_H
