/*
 * Arranges sibling elements in a list (horizontal or vertical)
 */

#ifndef AMETHYST__UI_LIST_LAYOUT_H
#define AMETHYST__UI_LIST_LAYOUT_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace Amethyst {

class Instance;

class UIListLayout : public UIExtension {
  public:
    explicit UIListLayout(UIObject *owner) : UIExtension(owner) {}
    virtual ~UIListLayout() = default;

    void apply(const std::vector<std::unique_ptr<Instance>> &children);

    /**
     * @brief How much room the visible children take laid out at the sizes they ask for, spacing included
     * @param children The children to measure
     * @param ownerSize The size their scale parts resolve against
     * @return The extent along the main axis and the largest child across it
     */
    vec2 measureNaturalContent(const std::vector<std::unique_ptr<Instance>> &children, vec2 ownerSize) const;

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

  private:
    struct SizeRecord {
        UDim2 natural; // the size the child asked for
        UDim2 written; // the size this layout last gave it
    };

    /**
     * @brief The size a child asked for, before any fill stretched it
     */
    UDim2 naturalSize(UIObject *obj);

    std::unordered_map<UIObject *, SizeRecord> m_sizes;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LIST_LAYOUT_H
