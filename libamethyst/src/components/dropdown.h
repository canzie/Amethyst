#ifndef AMETHYST__DROPDOWN_H
#define AMETHYST__DROPDOWN_H

#include "components/context_menu.h"
#include "components/context_menu_item.h"
#include "components/properties.h"
#include "components/text_button.h"
#include "modules/event_signal.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

class Dropdown : public TextButton {
  public:
    Dropdown();

    void resolveStyle() override;

    void setItems(std::vector<ContextMenuItem> items);
    std::vector<ContextMenuItem> &items();

    void open();
    void close();
    bool isOpen() const;

    bool setDropdownProperties(const DropdownStylePropertiesArgs &props);
    const DropdownStyleProperties &getDropdownProperties() const { return m_ddProps; }

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  protected:
    DropdownStyleProperties m_ddProps;

    EventResult onMouseButton1Down(int32_t x, int32_t y) override;

  private:
    void syncContextMenu();

    std::vector<ContextMenuItem> m_items;
    ContextMenu *m_contextMenu = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_H
