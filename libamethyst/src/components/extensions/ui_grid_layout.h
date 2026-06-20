/*
 * Arranges sibling elements in a grid
 */

#ifndef AMETHYST__UI_GRID_LAYOUT_H
#define AMETHYST__UI_GRID_LAYOUT_H

#include "components/common.h"
#include "components/extensions/ui_extension.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {
class Instance;

class UIGridLayout : public UIExtension {
  public:
    explicit UIGridLayout(UIObject *owner) : UIExtension(owner) {}
    virtual ~UIGridLayout() = default;

    void apply(const std::vector<std::unique_ptr<Instance>> &children);

  public:
    FillDirection fillDirection = FillDirection::FILL_VERTICAL;
    HorizontalAlignment horizontalAlignment = HorizontalAlignment::ALIGN_LEFT;
    VerticalAlignment verticalAlignment = VerticalAlignment::ALIGN_TOP;
    SortOrder sortOrder = SortOrder::SORT_LAYOUT_ORDER;
    UDim2 cellPadding;
    UDim2 cellSize;
    StartCorner startCorner;
    uint32_t fillDirectionMaxCells = 0; // 0=auto,

    bool flexCells = false;
    float maxCellWidth = 0.0f;
    float cellAspectRatio = 0.0f;

  private:
    vec2 m_absoluteCellSize;
};

} // namespace Amethyst

#endif // AMETHYST__UI_GRID_LAYOUT_H
