#include "components/extensions/ui_grid_layout.h"

#include "components/common.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>

namespace Amethyst {

void UIGridLayout::apply(const std::vector<std::unique_ptr<Instance>> &children)
{
    std::vector<UIObject *> items;
    items.reserve(children.size());
    for (auto &c : children) {
        auto *obj = c->as<UIObject>();
        if (obj != nullptr && obj->getBaseProperties().visible != 0) {
            items.push_back(obj);
        }
    }

    if (items.empty()) return;

    std::sort(items.begin(), items.end(), [this](UIObject *a, UIObject *b) {
        switch (sortOrder) {
        case SortOrder::SORT_LAYOUT_ORDER:
            return a->getBaseProperties().layoutOrder < b->getBaseProperties().layoutOrder;
        case SortOrder::SORT_NAME:
            return a->name < b->name;
        }
        return false;
    });

    uint32_t visibleCount = static_cast<uint32_t>(items.size());

    vec2 containerSize = m_owner->absoluteContentSize;
    m_absoluteCellSize = cellSize.resolve(containerSize);
    vec2 absCellPadding = cellPadding.resolve(containerSize);

    bool isVertical = (fillDirection == FillDirection::FILL_VERTICAL);

    uint32_t maxCells = fillDirectionMaxCells;
    if (maxCells == 0) {
        if (isVertical) {
            float availableHeight = containerSize.y;
            if (m_absoluteCellSize.y + absCellPadding.y > 0) {
                maxCells = static_cast<uint32_t>((availableHeight + absCellPadding.y) / (m_absoluteCellSize.y + absCellPadding.y));
            }
        } else {
            float availableWidth = containerSize.x;
            if (m_absoluteCellSize.x + absCellPadding.x > 0) {
                maxCells = static_cast<uint32_t>((availableWidth + absCellPadding.x) / (m_absoluteCellSize.x + absCellPadding.x));
            }
        }
        if (maxCells == 0) maxCells = 1;
    }

    if (flexCells && !isVertical) {
        float minW = m_absoluteCellSize.x;
        uint32_t cols = std::max(1u, maxCells);
        float cellW = (containerSize.x - (cols - 1) * absCellPadding.x) / cols;
        if (maxCellWidth > 0.0f) {
            while (cellW > maxCellWidth) {
                float next = (containerSize.x - cols * absCellPadding.x) / (cols + 1);
                if (next < minW) break;
                cols++;
                cellW = next;
            }
        }
        maxCells = cols;
        m_absoluteCellSize.x = cellW;
        if (cellAspectRatio > 0.0f) {
            m_absoluteCellSize.y = cellW / cellAspectRatio;
        }
    }

    for (size_t childIndex = 0; childIndex < items.size(); childIndex++) {
        UIObject *obj = items[childIndex];

        uint32_t mainIndex = childIndex % maxCells;
        uint32_t crossIndex = childIndex / maxCells;

        uint32_t row, col;
        if (isVertical) {
            row = mainIndex;
            col = crossIndex;
        } else {
            row = crossIndex;
            col = mainIndex;
        }

        float cellX = col * (m_absoluteCellSize.x + absCellPadding.x);
        float cellY = row * (m_absoluteCellSize.y + absCellPadding.y);

        switch (startCorner) {
        case StartCorner::TOP_LEFT:
            break;
        case StartCorner::TOP_RIGHT:
            cellX = containerSize.x - m_absoluteCellSize.x - cellX;
            break;
        case StartCorner::BOTTOM_LEFT:
            cellY = containerSize.y - m_absoluteCellSize.y - cellY;
            break;
        case StartCorner::BOTTOM_RIGHT:
            cellX = containerSize.x - m_absoluteCellSize.x - cellX;
            cellY = containerSize.y - m_absoluteCellSize.y - cellY;
            break;
        }

        float alignOffsetX = 0.0f;
        float alignOffsetY = 0.0f;

        switch (horizontalAlignment) {
        case HorizontalAlignment::ALIGN_LEFT:
            break;
        case HorizontalAlignment::ALIGN_CENTER_H: {
            // Center the whole row block, not a single cell, so leftover space splits evenly
            // instead of pooling on the right.
            uint32_t cellsInRow = isVertical ? 1u : std::min<uint32_t>(maxCells, visibleCount - crossIndex * maxCells);
            float rowWidth = cellsInRow * m_absoluteCellSize.x + (cellsInRow > 1 ? cellsInRow - 1 : 0) * absCellPadding.x;
            alignOffsetX = (containerSize.x - rowWidth) / 2.0f;
            break;
        }
        case HorizontalAlignment::ALIGN_RIGHT:
            alignOffsetX = m_owner->absoluteContentSize.x - m_absoluteCellSize.x;
            break;
        }

        obj->setBaseProperties({
            .position = UDim2::fromOffset(cellX + alignOffsetX, cellY + alignOffsetY),
            .size = UDim2::fromOffset(m_absoluteCellSize.x, m_absoluteCellSize.y),
        });
        obj->markDirty();
    }
}

} // namespace Amethyst
