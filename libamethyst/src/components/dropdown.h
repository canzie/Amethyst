/*
 * Dropdown selection UI element
 *
 * Structure:
 * Dropdown (UIObject) - main button showing current selection
 *   └─ m_popup (ScrollingFrame*) - child container for options
 *        ├─ UIListLayout extension - arranges children vertically
 *        └─ TextButton children (m_optionButtons) - one per option
 *
 * On click: toggles m_popup visibility and positioning
 * updateOptions(): rebuilds TextButton children from optionsRef
 */

#ifndef AMETHYST__DROPDOWN_H
#define AMETHYST__DROPDOWN_H

#include "components/common.h"
#include "components/ui_object.h"
#include <functional>
#include <string>
#include <vector>

namespace Amethyst {

struct Font;
class ScrollingFrame;
class TextButton;

enum class DropdownDirection {
    DOWN,
    UP,
    LEFT,
    RIGHT
};

class Dropdown : public UIObject {
  public:
    Dropdown();
    virtual ~Dropdown() = default;

    void draw(DrawContext &ctx) override;
    void updateOptions();

  protected:
    EventResult onMouseButton1Click() override;

  public:
    std::vector<std::string> *optionsRef = nullptr;
    int *selectedIndexRef = nullptr;
    std::function<void(int, const std::string &)> onSelectionChanged;

    std::string label;
    Font *font = nullptr;
    Color4 labelColor = {0.0f, 0.0f, 0.0f, 1.0f};
    LabelSide labelSide = LabelSide::LEFT;
    UDim labelPadding = UDim::fromOffset(5.0f);

    DropdownDirection popupDirection = DropdownDirection::DOWN;
    Color4 optionTextColor = {0.0f, 0.0f, 0.0f, 1.0f};
    Color3 highlightColor = {0.7f, 0.7f, 0.9f};
    float highlightTransparency = 0.0f;

  private:
    ScrollingFrame *m_popup = nullptr;
    std::vector<TextButton *> m_optionButtons;
    bool m_popupOpen = false;
};

} // namespace Amethyst

#endif // AMETHYST__DROPDOWN_H
