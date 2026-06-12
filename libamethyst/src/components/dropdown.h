/*
 * Dropdown menu button
 *
 * Renders as a TextButton trigger; clicking opens a floating popup panel in the
 * OverlayLayer. Popup is a Frame or ScrollingFrame (when itemCount > maxVisibleItems)
 * containing one TextButton row per item. Submenus open as a sibling panel in the
 * same OverlayLayer.
 */

#ifndef AMETHYST__DROPDOWN_H
#define AMETHYST__DROPDOWN_H

#include "components/dropdown_item.h"
#include "components/properties.h"
#include "components/text_button.h"
#include "modules/color.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

class Instance;
class InvisibleButton;
class OverlayLayer;
class UIObject;

class Dropdown : public TextButton {
  public:
    Dropdown();
    ~Dropdown() override;

    void draw(DrawContext &ctx) override;

    void setItems(std::vector<DropdownItem> items);
    std::vector<DropdownItem> &items() { return m_items; }

    void open();
    void requestClose();
    void closeImmediate();
    bool isOpen() const { return m_state == State::OPEN; }

    bool setDropdownProperties(const DropdownStyleProperties &props);
    const DropdownStyleProperties &getDropdownProperties() const { return m_ddProps; }

    std::function<void(std::string_view)> onItemSelected;
    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  protected:
    DropdownStyleProperties m_ddProps;

    EventResult onMouseButton1Down(uint32_t x, uint32_t y) override;

  private:
    void actuallyClose();
    void closeSubmenuFrom(size_t depth = 0);
    void buildMainPopup(OverlayLayer *overlay);
    void buildSubmenuAtPath(OverlayLayer *overlay, const std::vector<size_t> &path, vec2 pos);
    UIObject *buildPopupPanel(OverlayLayer *overlay, vec2 pos, float totalHeight, float visibleHeight, int zIdx,
                              const std::vector<size_t> &path = {});
    void addItemRows(Instance *container, int zIdx, const std::vector<size_t> &path = {});
    std::vector<DropdownItem> &itemsAtPath(const std::vector<size_t> &path);
    std::string buildItemText(const DropdownItem &item) const;
    float computeTotalHeight(const std::vector<DropdownItem> &items) const;

    enum class State {
        CLOSED,
        OPEN,
        PENDING_CLOSE
    };

    std::vector<DropdownItem> m_items;
    State m_state = State::CLOSED;

    UIObject *m_popup = nullptr;
    std::vector<UIObject *> m_submenuStack;
    std::vector<TextButton *> m_submenuSourceRows;
    InvisibleButton *m_eater = nullptr;
    OverlayLayer *m_overlayPtr = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_H
