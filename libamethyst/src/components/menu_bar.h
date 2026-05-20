/*
 * Horizontal strip of top-level Dropdown menus.
 *
 * Each entry is a Dropdown with maxVisibleItems=INT_MAX (no scroll). Hovering
 * an entry while another is open immediately switches focus.
 */

#ifndef AMETHYST__MENU_BAR_H
#define AMETHYST__MENU_BAR_H

#include "components/dropdown.h"
#include "components/dropdown_item.h"
#include "components/frame.h"

#include <string>
#include <vector>

namespace Amethyst {

class MenuBar : public Frame {
  public:
    MenuBar();
    ~MenuBar() override = default;

    void draw(DrawContext &ctx) override;

    Dropdown *addMenu(std::string label, std::vector<DropdownItem> items);
    void clear();

    float entryPaddingX = 12.0f;
    float entryPaddingY = 4.0f;
    float entryFontSize = 14.0f;
    Color3 entryHoverBackground = {0.25f, 0.25f, 0.30f};
    Color3 entryActiveBackground = {0.28f, 0.42f, 0.62f};

  private:
    void onEntryHovered(Dropdown *entry);
    void onEntryClosed(Dropdown *entry);

    std::vector<Dropdown *> m_entries;
    Dropdown *m_openEntry = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__MENU_BAR_H
