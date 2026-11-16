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

    std::stable_sort(items.begin(), items.end(), [this](UIObject *a, UIObject *b) {
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

    // the block of cells as laid out from the top-left, so it can be aligned as a whole and mirrored per start corner
    uint32_t lineCount = (visibleCount + maxCells - 1) / maxCells;
    vec2 stride = m_absoluteCellSize + absCellPadding;
    auto extent = [](uint32_t cells, float cell, float padding) {
        return cells == 0 ? 0.0f : static_cast<float>(cells) * cell + static_cast<float>(cells - 1) * padding;
    };
    auto alignFactorX = [this] {
        switch (horizontalAlignment) {
        case HorizontalAlignment::ALIGN_CENTER_H:
            return 0.5f;
        case HorizontalAlignment::ALIGN_RIGHT:
            return 1.0f;
        default:
            return 0.0f;
        }
    };
    auto alignFactorY = [this] {
        switch (verticalAlignment) {
        case VerticalAlignment::ALIGN_CENTER_V:
            return 0.5f;
        case VerticalAlignment::ALIGN_BOTTOM:
            return 1.0f;
        default:
            return 0.0f;
        }
    };
    bool mirrorX = startCorner == StartCorner::TOP_RIGHT || startCorner == StartCorner::BOTTOM_RIGHT;
    bool mirrorY = startCorner == StartCorner::BOTTOM_LEFT || startCorner == StartCorner::BOTTOM_RIGHT;

    for (size_t childIndex = 0; childIndex < items.size(); childIndex++) {
        UIObject *obj = items[childIndex];

        uint32_t mainIndex = static_cast<uint32_t>(childIndex) % maxCells;
        uint32_t crossIndex = static_cast<uint32_t>(childIndex) / maxCells;
        // a partly filled last line is aligned on its own
        uint32_t cellsInLine = std::min<uint32_t>(maxCells, visibleCount - crossIndex * maxCells);

        uint32_t row = isVertical ? mainIndex : crossIndex;
        uint32_t col = isVertical ? crossIndex : mainIndex;
        float blockW = isVertical ? extent(lineCount, m_absoluteCellSize.x, absCellPadding.x)
                                  : extent(cellsInLine, m_absoluteCellSize.x, absCellPadding.x);
        float blockH = isVertical ? extent(cellsInLine, m_absoluteCellSize.y, absCellPadding.y)
                                  : extent(lineCount, m_absoluteCellSize.y, absCellPadding.y);

        float cellX = static_cast<float>(col) * stride.x;
        float cellY = static_cast<float>(row) * stride.y;
        if (mirrorX) {
            cellX = blockW - m_absoluteCellSize.x - cellX;
        }
        if (mirrorY) {
            cellY = blockH - m_absoluteCellSize.y - cellY;
        }

        float alignOffsetX = (containerSize.x - blockW) * alignFactorX();
        float alignOffsetY = (containerSize.y - blockH) * alignFactorY();

        obj->setBaseProperties({
            .position = UDim2::fromOffset(cellX + alignOffsetX, cellY + alignOffsetY),
            .size = UDim2::fromOffset(m_absoluteCellSize.x, m_absoluteCellSize.y),
        });
        obj->markDirty();
    }
}

} // namespace Amethyst
