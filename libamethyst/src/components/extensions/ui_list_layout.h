/*
 * Arranges sibling elements in a list (horizontal or vertical)
 */

#ifndef AMETHYST__UI_LIST_LAYOUT_H
#define AMETHYST__UI_LIST_LAYOUT_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
#include <vector>

namespace Amethyst {

class Instance;

class UIListLayout : public UIExtension {
  public:
    explicit UIListLayout(UIObject *owner) : UIExtension(owner) {}
    virtual ~UIListLayout() = default;

    void apply(std::vector<Instance *> &children);

  public:
    FillDirection fillDirection = FillDirection::FILL_VERTICAL;
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::ALIGN_LEFT;
    VerticalAlignment verticalAlignment = VerticalAlignment::ALIGN_TOP;
    SortOrder sortOrder = SortOrder::SORT_LAYOUT_ORDER;
    UDim innerPadding;
    UiFlexAlignment horizontalFlex = UiFlexAlignment::NONE;
    UiFlexAlignment verticalFlex = UiFlexAlignment::NONE;
    bool wraps = false;
    ItemLineAlignment itemLineAlignment = ItemLineAlignment::AUTOMATIC;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LIST_LAYOUT_H
