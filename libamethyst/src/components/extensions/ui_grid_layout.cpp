#include "components/extensions/ui_grid_layout.h"

#include "components/common.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>

namespace Amethyst {

void UIGridLayout::apply(const std::vector<std::unique_ptr<Instance>> &children)
{
    std::vector<Instance *> sortedChildren;
    sortedChildren.reserve(children.size());
    for (auto &c : children) sortedChildren.push_back(c.get());
    std::sort(sortedChildren.begin(), sortedChildren.end(), [this](Instance *a, Instance *b) {
        auto *aObj = a->as<UIObject>();
        auto *bObj = b->as<UIObject>();

        if (aObj && bObj) {
            switch (sortOrder) {
            case SortOrder::SORT_LAYOUT_ORDER:
                return aObj->getBaseProperties().layoutOrder < bObj->getBaseProperties().layoutOrder;
            case SortOrder::SORT_NAME:
                return a->name < b->name;
            };
        }

        return false;
    });

    if (sortedChildren.empty()) return;

    glm::vec2 containerSize = m_owner->absoluteContentSize;
    m_absoluteCellSize = cellSize.resolve(containerSize);
    glm::vec2 absCellPadding = cellPadding.resolve(containerSize);

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

    size_t childIndex = 0;
    for (auto child : sortedChildren) {
        auto *obj = child->as<UIObject>();
        if (obj == nullptr) continue;
        if (obj->getBaseProperties().visible == 0) continue;

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
        case HorizontalAlignment::ALIGN_CENTER_H:
            alignOffsetX = (m_owner->absoluteContentSize.x - m_absoluteCellSize.x) / 2.0f;
            break;
        case HorizontalAlignment::ALIGN_RIGHT:
            alignOffsetX = m_owner->absoluteContentSize.x - m_absoluteCellSize.x;
            break;
        }

        obj->setBaseProperties({
            .position = UDim2::fromOffset(cellX + alignOffsetX, cellY + alignOffsetY),
            .size = UDim2::fromOffset(m_absoluteCellSize.x, m_absoluteCellSize.y),
        });
        child->markDirty();

        childIndex++;
    }
}

} // namespace Amethyst
