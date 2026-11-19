#ifndef AMETHYST__DROPDOWN_H
#define AMETHYST__DROPDOWN_H

#include "components/context_menu.h"
#include "components/properties.h"
#include "components/text_button.h"

#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

class ImageLabel;

class Dropdown : public TextButton {
  public:
    Dropdown();

    void resolveStyle() override;

    void setItems(std::vector<std::unique_ptr<ContextMenu::ItemData>> items);
    std::vector<std::unique_ptr<ContextMenu::ItemData>> &items();

    void open();
    void close();
    bool isOpen() const;

    bool setDropdownProperties(const DropdownStylePropertiesArgs &props);
    const DropdownStyleProperties &getDropdownProperties() const { return m_ddProps; }

    /**
     * @brief Gives access to the arrow shown in the right padding, to change its icon
     * @param fn Called with the arrow
     * @return This dropdown
     */
    Dropdown &indicator(std::function<void(ImageLabel &)> fn);

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  protected:
    DropdownStyleProperties m_ddProps;

    EventResult onMouseButton1Down(int32_t x, int32_t y) override;

  private:
    void syncContextMenu();

    /**
     * @brief Places the arrow in the right padding and tints it, after the size, padding or color changed
     */
    void placeIndicator();

    std::vector<std::unique_ptr<ContextMenu::ItemData>> m_items;
    ContextMenu *m_contextMenu = nullptr;
    ImageLabel *m_indicator = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_H
