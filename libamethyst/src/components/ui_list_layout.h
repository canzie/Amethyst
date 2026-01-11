/*
 * Arranges sibling elements in a list (horizontal or vertical)
 */

#ifndef AMETHYST__UI_LIST_LAYOUT_H
#define AMETHYST__UI_LIST_LAYOUT_H

#include "components/ui_component.h"
#include "components/common.h"

namespace Amethyst {

enum FillDirection : uint8_t {
    FILL_HORIZONTAL,
    FILL_VERTICAL,
};

enum HorizontalAlignment : uint8_t {
    ALIGN_LEFT,
    ALIGN_CENTER_H,
    ALIGN_RIGHT,
};

enum VerticalAlignment : uint8_t {
    ALIGN_TOP,
    ALIGN_CENTER_V,
    ALIGN_BOTTOM,
};

enum SortOrder : uint8_t {
    SORT_NAME,
    SORT_LAYOUT_ORDER,
};

class UIListLayout : public UIComponent {
public:
    UIListLayout() = default;
    virtual ~UIListLayout() = default;

    void apply() override;

public:
    FillDirection fillDirection = FILL_VERTICAL;
    HorizontalAlignment horizontalAlignment = ALIGN_LEFT;
    VerticalAlignment verticalAlignment = ALIGN_TOP;
    SortOrder sortOrder = SORT_LAYOUT_ORDER;
    UDim padding;
};

} // namespace Amethyst

#endif // AMETHYST__UI_LIST_LAYOUT_H
