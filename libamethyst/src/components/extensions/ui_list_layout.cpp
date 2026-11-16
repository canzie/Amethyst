#include "components/extensions/ui_list_layout.h"

#include "components/common.h"
#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_size_constraint.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>
#include <unordered_set>

namespace Amethyst {

/**
 * @brief What UIObject::resolveSize gives, but for a size the child asked for rather than the one it holds now
 */
static vec2 s_resolveNaturalSize(const UIObject &obj, const UDim2 &size, vec2 parentSize)
{
    vec2 resolved = size.resolve(parentSize);
    vec4 m = obj.getBaseProperties().margin.resolve(parentSize);
    resolved -= vec2(m.w + m.y, m.x + m.z);
    if (const auto *sizeConstraint = obj.getExtension<UISizeConstraint>(); sizeConstraint != nullptr) {
        resolved = sizeConstraint->constrain(resolved);
    }
    if (const auto *arConstraint = obj.getExtension<UIAspectRatioConstraint>(); arConstraint != nullptr) {
        resolved = arConstraint->constrain(resolved);
    }
    return resolved;
}

UDim2 UIListLayout::naturalSize(UIObject *obj)
{
    UDim2 current = obj->getBaseProperties().size;
    auto it = m_sizes.find(obj);
    // a size other than the one this layout last wrote was set by someone else and becomes the new natural size
    if (it == m_sizes.end() || !(it->second.written == current)) {
        m_sizes[obj] = SizeRecord{current, current};
        return current;
    }
    return it->second.natural;
}

void UIListLayout::apply(const std::vector<std::unique_ptr<Instance>> &children)
{
    // hidden children take no room, matching UIGridLayout
    std::vector<UIObject *> sortedChildren;
    sortedChildren.reserve(children.size());
    for (auto &c : children) {
        if (auto *obj = c->asUiObject(); obj != nullptr && static_cast<bool>(obj->getBaseProperties().visible)) {
            sortedChildren.push_back(obj);
        }
    }
    // stable so children with equal keys keep their insertion order from one layout pass to the next
    std::stable_sort(sortedChildren.begin(), sortedChildren.end(), [this](UIObject *a, UIObject *b) {
        switch (sortOrder) {
        case SortOrder::SORT_LAYOUT_ORDER:
            return a->getBaseProperties().layoutOrder < b->getBaseProperties().layoutOrder;
        case SortOrder::SORT_NAME:
            return a->name < b->name;
        }
        return false;
    });

    // forget children that left, so a recycled address never inherits a stale natural size
    std::unordered_set<UIObject *> present(sortedChildren.begin(), sortedChildren.end());
    std::erase_if(m_sizes, [&present](const auto &entry) { return !present.contains(entry.first); });

    if (sortedChildren.empty()) return;

    bool isVertical = (fillDirection == FillDirection::FILL_VERTICAL);
    vec2 ownerSize = m_owner->absoluteContentSize;
    float containerSize = isVertical ? ownerSize.y : ownerSize.x;
    float crossContainerSize = isVertical ? ownerSize.x : ownerSize.y;
    float absPadding = innerPadding.resolve(containerSize);

    UiFlexAlignment mainAxisFlex = isVertical ? verticalFlex : horizontalFlex;
    UiFlexAlignment crossAxisFlex = isVertical ? horizontalFlex : verticalFlex;

    std::vector<UDim2> naturalSizes;
    naturalSizes.reserve(sortedChildren.size());
    float totalChildSize = 0.0f;
    size_t validChildren = sortedChildren.size();
    for (UIObject *obj : sortedChildren) {
        naturalSizes.push_back(naturalSize(obj));
        vec2 childSize = s_resolveNaturalSize(*obj, naturalSizes.back(), ownerSize);
        totalChildSize += isVertical ? childSize.y : childSize.x;
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
        // negative when the children ask for more than there is, so they shrink instead of overlapping
        if (validChildren > 0) {
            extraItemSize = remainingSpace / static_cast<float>(validChildren);
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

    bool mainFill = mainAxisFlex == UiFlexAlignment::FILL;
    for (size_t idx = 0; idx < sortedChildren.size(); idx++) {
        UIObject *obj = sortedChildren[idx];
        const UDim2 &natural = naturalSizes[idx];

        vec2 childSize = s_resolveNaturalSize(*obj, natural, ownerSize);
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

        float mainSize = std::max(0.0f, (isVertical ? childSize.y : childSize.x) + extraItemSize);
        // the child keeps the size it asked for unless a fill stretches it; the stretched size is written as an
        // offset, and the size it asked for is remembered so it can shrink back
        UDim2 size = natural;
        if (mainFill || crossAxisFlex == UiFlexAlignment::FILL) {
            // resolveSize takes the margin back off, so the written size carries it
            vec4 m = obj->getBaseProperties().margin.resolve(ownerSize);
            vec2 filled = isVertical ? vec2(childSize.x, mainSize) : vec2(mainSize, childSize.y);
            filled += vec2(m.w + m.y, m.x + m.z);
            size = UDim2::fromOffset(filled.x, filled.y);
        }

        BasePropertiesArgs props;
        props.position = isVertical ? UDim2::fromOffset(crossOffset, currentOffset) : UDim2::fromOffset(currentOffset, crossOffset);
        props.size = size;
        obj->setBaseProperties(props);
        m_sizes[obj].written = size;
        currentOffset += mainSize + spacing;

        obj->markDirty();
    }
}

} // namespace Amethyst
