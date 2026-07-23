/*
 * Horizontal strip of top-level Dropdown menus.
 *
 * Each entry is a Dropdown with maxVisibleItems=INT_MAX (no scroll). Hovering
 * an entry while another is open immediately switches focus.
 */

#ifndef AMETHYST__MENU_BAR_H
#define AMETHYST__MENU_BAR_H

#include "components/context_menu_item.h"
#include "components/dropdown.h"
#include "components/frame.h"
#include "components/properties.h"

#include <string>
#include <vector>

namespace Amethyst {

class MenuBar : public Frame {
  public:
    MenuBar();
    ~MenuBar() override = default;

    Dropdown *addMenu(std::string label, std::vector<ContextMenuItem> items);
    void clear();

    bool setMenuBarProperties(const MenuBarStyleProperties &props);
    const MenuBarStyleProperties &getMenuBarProperties() const { return m_mbProps; }

    void resolveStyle() override;

  protected:
    MenuBarStyleProperties m_mbProps;

  private:
    void onEntryHovered(Dropdown *entry);
    void onEntryClosed(Dropdown *entry);

    std::vector<Dropdown *> m_entries;
    Dropdown *m_openEntry = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__MENU_BAR_H
