/*
 * UIListLayout implementation
 */

#include "components/extensions/ui_list_layout.h"

#include "components/common.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>

namespace Amethyst {

void UIListLayout::apply(std::vector<Instance *> &children)
{
    std::vector<Instance *> sortedChildren(children.begin(), children.end());
    std::sort(sortedChildren.begin(), sortedChildren.end(), [this](Instance *a, Instance *b) {
        auto *aObj = a->as<UIObject>();
        auto *bObj = b->as<UIObject>();

        if (aObj && bObj) {
            switch (sortOrder) {
            case SortOrder::SORT_LAYOUT_ORDER:
                return aObj->layoutOrder < bObj->layoutOrder;
            case SortOrder::SORT_NAME:
                return a->name < b->name;
            };
        }

        return false;
    });

    if (sortedChildren.empty()) return;

    bool isVertical = (fillDirection == FillDirection::FILL_VERTICAL);
    float containerSize = isVertical ? m_owner->absoluteSize.y : m_owner->absoluteSize.x;
    float crossContainerSize = isVertical ? m_owner->absoluteSize.x : m_owner->absoluteSize.y;
    float absPadding = innerPadding.resolve(containerSize);

    UiFlexAlignment mainAxisFlex = isVertical ? verticalFlex : horizontalFlex;
    UiFlexAlignment crossAxisFlex = isVertical ? horizontalFlex : verticalFlex;

    float totalChildSize = 0.0f;
    size_t validChildren = 0;
    for (auto child : sortedChildren) {
        auto *obj = child->as<UIObject>();
        if (obj != nullptr) {
            totalChildSize += isVertical ? obj->absoluteSize.y : obj->absoluteSize.x;
            validChildren++;
        }
    }

    float totalPadding = validChildren > 1 ? absPadding * (validChildren - 1) : 0.0f;
    float remainingSpace = containerSize - totalChildSize - totalPadding;
    float currentOffset = 0.0f;
    float spacing = absPadding;
    float extraItemSize = 0.0f;

    switch (mainAxisFlex) {
    case UiFlexAlignment::NONE:
        currentOffset = 0.0f;
        spacing = absPadding;
        break;
    case UiFlexAlignment::FILL:
        if (validChildren > 0) {
            extraItemSize = remainingSpace / validChildren;
        }
        currentOffset = 0.0f;
        spacing = absPadding;
        break;
    case UiFlexAlignment::SPACE_BETWEEN:
        if (validChildren > 1) {
            spacing = absPadding + remainingSpace / (validChildren - 1);
        }
        currentOffset = 0.0f;
        break;
    case UiFlexAlignment::SPACE_AROUND:
        if (validChildren > 0) {
            spacing = absPadding + remainingSpace / validChildren;
            currentOffset = spacing / 2.0f;
        }
        break;
    case UiFlexAlignment::SPACE_EVENLY:
        if (validChildren > 0) {
            float totalSpace = absPadding * (validChildren - 1) + remainingSpace;
            spacing = totalSpace / (validChildren + 1);
            currentOffset = spacing;
        }
        break;
    }

    for (auto child : sortedChildren) {
        auto *obj = child->as<UIObject>();
        if (obj == nullptr) continue;

        float crossOffset = 0.0f;

        if (crossAxisFlex == UiFlexAlignment::FILL) {
            crossOffset = 0.0f;
            if (isVertical) {
                obj->size = UDim2::fromOffset(crossContainerSize, obj->absoluteSize.y);
            } else {
                obj->size = UDim2::fromOffset(obj->absoluteSize.x, crossContainerSize);
            }
        } else {
            float itemCrossSize = isVertical ? obj->absoluteSize.x : obj->absoluteSize.y;
            if (isVertical) {
                switch (horizontalAlignment) {
                case HorizontalAlignment::ALIGN_LEFT:
                    crossOffset = 0.0f;
                    break;
                case HorizontalAlignment::ALIGN_CENTER_H:
                    crossOffset = (crossContainerSize - itemCrossSize) / 2.0f;
                    break;
                case HorizontalAlignment::ALIGN_RIGHT:
                    crossOffset = crossContainerSize - itemCrossSize;
                    break;
                }
            } else {
                switch (verticalAlignment) {
                case VerticalAlignment::ALIGN_TOP:
                    crossOffset = 0.0f;
                    break;
                case VerticalAlignment::ALIGN_CENTER_V:
                    crossOffset = (crossContainerSize - itemCrossSize) / 2.0f;
                    break;
                case VerticalAlignment::ALIGN_BOTTOM:
                    crossOffset = crossContainerSize - itemCrossSize;
                    break;
                }
            }
        }

        if (isVertical) {
            obj->position = UDim2::fromOffset(crossOffset, currentOffset);
            float itemSize = obj->absoluteSize.y + extraItemSize;
            if (extraItemSize > 0.0f) {
                obj->size = UDim2::fromOffset(obj->absoluteSize.x, itemSize);
            }
            currentOffset += itemSize + spacing;
        } else {
            obj->position = UDim2::fromOffset(currentOffset, crossOffset);
            float itemSize = obj->absoluteSize.x + extraItemSize;
            if (extraItemSize > 0.0f) {
                obj->size = UDim2::fromOffset(itemSize, obj->absoluteSize.y);
            }
            currentOffset += itemSize + spacing;
        }

        child->markDirty();
    }
}

} // namespace Amethyst
