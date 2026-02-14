#include "components/extensions/ui_list_layout.h"

#include "components/common.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>

namespace Amethyst {

void UIListLayout::apply(const std::vector<std::unique_ptr<Instance>> &children)
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
                return aObj->layoutOrder < bObj->layoutOrder;
            case SortOrder::SORT_NAME:
                return a->name < b->name;
            };
        }

        return false;
    });

    if (sortedChildren.empty()) return;

    bool isVertical = (fillDirection == FillDirection::FILL_VERTICAL);
    glm::vec2 ownerSize = m_owner->absoluteSize;
    float containerSize = isVertical ? ownerSize.y : ownerSize.x;
    float crossContainerSize = isVertical ? ownerSize.x : ownerSize.y;
    float absPadding = innerPadding.resolve(containerSize);

    UiFlexAlignment mainAxisFlex = isVertical ? verticalFlex : horizontalFlex;
    UiFlexAlignment crossAxisFlex = isVertical ? horizontalFlex : verticalFlex;

    float totalChildSize = 0.0f;
    size_t validChildren = 0;
    for (auto child : sortedChildren) {
        auto *obj = child->as<UIObject>();
        if (obj != nullptr) {
            glm::vec2 childSize = obj->size.resolve(ownerSize);
            totalChildSize += isVertical ? childSize.y : childSize.x;
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

        glm::vec2 childSize = obj->size.resolve(ownerSize);
        float crossOffset = 0.0f;

        if (crossAxisFlex == UiFlexAlignment::FILL) {
            crossOffset = 0.0f;
            if (isVertical) {
                obj->size = UDim2::fromOffset(crossContainerSize, childSize.y);
            } else {
                obj->size = UDim2::fromOffset(childSize.x, crossContainerSize);
            }
        } else {
            float itemCrossSize = isVertical ? childSize.x : childSize.y;
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
            float itemSize = childSize.y + extraItemSize;
            if (extraItemSize > 0.0f) {
                obj->size = UDim2::fromOffset(childSize.x, itemSize);
            }
            currentOffset += itemSize + spacing;
        } else {
            obj->position = UDim2::fromOffset(currentOffset, crossOffset);
            float itemSize = childSize.x + extraItemSize;
            if (extraItemSize > 0.0f) {
                obj->size = UDim2::fromOffset(itemSize, childSize.y);
            }
            currentOffset += itemSize + spacing;
        }

        child->markDirty();
    }
}

} // namespace Amethyst
