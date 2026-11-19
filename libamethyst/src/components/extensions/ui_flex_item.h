#ifndef AMETHYST__UI_FLEX_ITEM_H
#define AMETHYST__UI_FLEX_ITEM_H

#include "components/extensions/ui_extension.h"

namespace Amethyst {

/**
 * @brief Marks a child of a UIListLayout filling its main axis as one that takes a share of the space the others
 * leave, growing or shrinking within its UISizeConstraint; the children without one keep their own size
 */
class UIFlexItem : public UIExtension {
  public:
    explicit UIFlexItem(UIObject *owner) : UIExtension(owner) {}
    virtual ~UIFlexItem() = default;

  public:
    float flexRatio = 1.0f;
};

} // namespace Amethyst

#endif // AMETHYST__UI_FLEX_ITEM_H
