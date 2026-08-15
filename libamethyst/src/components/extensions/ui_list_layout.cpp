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
                return aObj->getBaseProperties().layoutOrder < bObj->getBaseProperties().layoutOrder;
            case SortOrder::SORT_NAME:
                return a->name < b->name;
            };
        }

        return false;
    });

    if (sortedChildren.empty()) return;

    bool isVertical = (fillDirection == FillDirection::FILL_VERTICAL);
    vec2 ownerSize = m_owner->absoluteContentSize;
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
            vec2 childSize = obj->resolveSize(ownerSize);
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

        vec2 childSize = obj->resolveSize(ownerSize);
        float crossOffset = 0.0f;

        if (crossAxisFlex == UiFlexAlignment::FILL) {
            if (isVertical) {
                childSize.x = crossContainerSize;
            } else {
                childSize.y = crossContainerSize;
            }
        } else {
            float itemCrossSize = isVertical ? childSize.x : childSize.y;
            if (isVertical) {
                switch (horizontalAlignment) {
                case HorizontalAlignment::ALIGN_LEFT:
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
            float itemSize = childSize.y + extraItemSize;
            BasePropertiesArgs props;
            props.position = UDim2::fromOffset(crossOffset, currentOffset);
            if (extraItemSize > 0.0f || crossAxisFlex == UiFlexAlignment::FILL) {
                props.size = UDim2::fromOffset(childSize.x, itemSize);
            }
            obj->setBaseProperties(props);
            currentOffset += itemSize + spacing;
        } else {
            float itemSize = childSize.x + extraItemSize;
            BasePropertiesArgs props;
            props.position = UDim2::fromOffset(currentOffset, crossOffset);
            if (extraItemSize > 0.0f || crossAxisFlex == UiFlexAlignment::FILL) {
                props.size = UDim2::fromOffset(itemSize, childSize.y);
            }
            obj->setBaseProperties(props);
            currentOffset += itemSize + spacing;
        }

        child->markDirty();
    }
}

} // namespace Amethyst
