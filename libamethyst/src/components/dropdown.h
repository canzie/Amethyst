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

enum class DropdownDirection {
    DOWN,
    UP,
    LEFT,
    RIGHT
};

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

    DropdownDirection popupDirection = DropdownDirection::DOWN;
    int maxVisibleItems = 8;
    float itemHeight = 24.0f;
    float popupWidth = 180.0f;
    float itemFontSize = 14.0f;

    Color3 popupBackground = {0.18f, 0.18f, 0.18f};
    Color4 itemTextColor = {0.92f, 0.92f, 0.92f, 1.0f};
    Color4 itemDisabledColor = {0.45f, 0.45f, 0.45f, 1.0f};
    Color3 itemHoverBackground = {0.25f, 0.42f, 0.65f};
    Color3 separatorColor = {0.32f, 0.32f, 0.32f};

    std::function<void()> onOpenedCb;
    std::function<void()> onClosedCb;

  protected:
    EventResult onMouseButton1Down(uint32_t x, uint32_t y) override;

  private:
    void actuallyClose();
    void closeSubmenuFrom(size_t depth = 0);
    void buildMainPopup(OverlayLayer *overlay);
    void buildSubmenuAtPath(OverlayLayer *overlay, const std::vector<size_t> &path, glm::vec2 pos);
    UIObject *buildPopupPanel(OverlayLayer *overlay, glm::vec2 pos, float totalHeight,
                              float visibleHeight, int zIdx, const std::vector<size_t> &path = {});
    void addItemRows(Instance *container, int zIdx, const std::vector<size_t> &path = {});
    std::vector<DropdownItem> &itemsAtPath(const std::vector<size_t> &path);
    std::string buildItemText(const DropdownItem &item) const;
    float computeTotalHeight(const std::vector<DropdownItem> &items) const;

    enum class State { CLOSED, OPEN, PENDING_CLOSE };

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
