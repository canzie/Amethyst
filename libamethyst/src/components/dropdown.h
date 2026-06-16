/*
 * Dropdown menu button.
 */

#ifndef AMETHYST__DROPDOWN_H
#define AMETHYST__DROPDOWN_H

#include "components/dropdown_item.h"
#include "components/properties.h"
#include "components/text_button.h"
#include "modules/event_signal.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

class Instance;
class OverlayLayer;
class Popup;
class UIObject;

class Dropdown : public TextButton {
  public:
    Dropdown();
    ~Dropdown() override;

    void setItems(std::vector<DropdownItem> items);
    std::vector<DropdownItem> &items() { return m_items; }

    void open();
    void requestClose();
    void closeImmediate();
    bool isOpen() const { return m_open; }

    bool setDropdownProperties(const DropdownStyleProperties &props);
    const DropdownStyleProperties &getDropdownProperties() const { return m_ddProps; }

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  protected:
    DropdownStyleProperties m_ddProps;

    EventResult onMouseButton1Down(int32_t x, int32_t y) override;

  private:
    void closeSubmenuFrom(size_t depth = 0);
    void buildMainPopup();
    void buildSubmenuAtPath(const std::vector<size_t> &path, UIObject *sourceRow);
    Popup *buildPopupPanel(Popup *&slot, float totalHeight, float visibleHeight, const std::vector<size_t> &path);
    void addItemRows(Instance *container, const std::vector<size_t> &path = {});
    std::vector<DropdownItem> &itemsAtPath(const std::vector<size_t> &path);
    std::string buildItemText(const DropdownItem &item) const;
    float computeTotalHeight(const std::vector<DropdownItem> &items) const;

    std::vector<DropdownItem> m_items;
    bool m_open = false;

    Popup *m_popup = nullptr;
    std::vector<Popup *> m_submenuStack;
    std::vector<TextButton *> m_submenuSourceRows;
    OverlayLayer *m_overlayPtr = nullptr;
    EventConnection m_pressConn;
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_H
