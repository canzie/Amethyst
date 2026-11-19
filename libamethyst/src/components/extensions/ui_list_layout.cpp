#include "components/extensions/ui_list_layout.h"

#include "components/common.h"
#include "components/extensions/ui_aspect_ratio_constraint.h"
#include "components/extensions/ui_flex_item.h"
#include "components/extensions/ui_size_constraint.h"
#include "components/instance.h"
#include "components/ui_object.h"
#include <algorithm>
#include <cmath>
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
    resolved = obj.applyAutomaticSize(resolved);
    if (const auto *sizeConstraint = obj.getExtension<UISizeConstraint>(); sizeConstraint != nullptr) {
        resolved = sizeConstraint->constrain(resolved);
    }
    if (const auto *arConstraint = obj.getExtension<UIAspectRatioConstraint>(); arConstraint != nullptr) {
        resolved = arConstraint->constrain(resolved);
    }
    return resolved;
}

/**
 * @brief Shares out space along the main axis between the flex items, by ratio and within their size constraints,
 * handing what a clamped item cannot take to the others
 * @param children The children being laid out
 * @param mainSizes Each child's size along the main axis
 * @param space The space to share, negative when the children ask for more than there is
 * @param isVertical Whether the main axis is vertical
 * @return How much each child grows along the main axis, zero for the children without a flex item
 */
static std::vector<float> s_shareFlexSpace(const std::vector<UIObject *> &children, const std::vector<float> &mainSizes, float space,
                                           bool isVertical)
{
    std::vector<float> extra(children.size(), 0.0f);
    std::vector<bool> active(children.size(), false);
    for (size_t i = 0; i < children.size(); i++) {
        const UIFlexItem *flex = children[i]->getExtension<UIFlexItem>();
        active[i] = flex != nullptr && flex->flexRatio > 0.0f;
    }

    // each pass hands the space out by ratio; an item that hits a limit keeps it and drops out, and what it could not
    // take goes round again, so at most one pass per item
    for (size_t pass = 0; pass < children.size() && std::abs(space) > 0.001f; pass++) {
        float totalRatio = 0.0f;
        for (size_t i = 0; i < children.size(); i++) {
            if (active[i]) {
                totalRatio += children[i]->getExtension<UIFlexItem>()->flexRatio;
            }
        }
        if (totalRatio <= 0.0f) {
            break;
        }

        float handedOut = 0.0f;
        bool anyClamped = false;
        for (size_t i = 0; i < children.size(); i++) {
            if (!active[i]) {
                continue;
            }
            const float current = mainSizes[i] + extra[i];
            float wanted = current + space * children[i]->getExtension<UIFlexItem>()->flexRatio / totalRatio;
            float limited = std::max(0.0f, wanted);
            if (const auto *limits = children[i]->getExtension<UISizeConstraint>()) {
                limited = std::clamp(limited, isVertical ? limits->minSize.y : limits->minSize.x,
                                     isVertical ? limits->maxSize.y : limits->maxSize.x);
            }
            if (limited != wanted) {
                active[i] = false;
                anyClamped = true;
            }
            extra[i] += limited - current;
            handedOut += limited - current;
        }
        space -= handedOut;
        if (!anyClamped) {
            break;
        }
    }
    return extra;
}

vec2 UIListLayout::measureNaturalContent(const std::vector<std::unique_ptr<Instance>> &children, vec2 ownerSize) const
{
    const bool isVertical = fillDirection == FillDirection::FILL_VERTICAL;
    const float padding = innerPadding.resolve(isVertical ? ownerSize.y : ownerSize.x);

    float mainExtent = 0.0f;
    float crossExtent = 0.0f;
    size_t count = 0;
    for (const auto &child : children) {
        const UIObject *obj = child->asUiObject();
        if (obj == nullptr || !static_cast<bool>(obj->getBaseProperties().visible)) {
            continue;
        }
        // a stretched child is measured at the size it asked for, not the one a fill gave it
        UDim2 natural = obj->getBaseProperties().size;
        if (auto it = m_sizes.find(const_cast<UIObject *>(obj)); it != m_sizes.end() && it->second.written == natural) {
            natural = it->second.natural;
        }
        const vec2 size = s_resolveNaturalSize(*obj, natural, ownerSize);
        mainExtent += isVertical ? size.y : size.x;
        crossExtent = std::max(crossExtent, isVertical ? size.x : size.y);
        count++;
    }
    if (count > 1) {
        mainExtent += padding * static_cast<float>(count - 1);
    }
    return isVertical ? vec2(crossExtent, mainExtent) : vec2(mainExtent, crossExtent);
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
    std::vector<float> mainSizes;
    naturalSizes.reserve(sortedChildren.size());
    mainSizes.reserve(sortedChildren.size());
    float totalChildSize = 0.0f;
    size_t validChildren = sortedChildren.size();
    bool hasFlexItems = false;
    for (UIObject *obj : sortedChildren) {
        naturalSizes.push_back(naturalSize(obj));
        vec2 childSize = s_resolveNaturalSize(*obj, naturalSizes.back(), ownerSize);
        mainSizes.push_back(isVertical ? childSize.y : childSize.x);
        totalChildSize += mainSizes.back();
        hasFlexItems = hasFlexItems || obj->getExtension<UIFlexItem>() != nullptr;
    }

    float totalPadding = validChildren > 1 ? absPadding * (validChildren - 1) : 0.0f;
    float remainingSpace = containerSize - totalChildSize - totalPadding;
    float currentOffset = 0.0f;
    float spacing = absPadding;
    std::vector<float> extraItemSizes(sortedChildren.size(), 0.0f);

    switch (mainAxisFlex) {
    case UiFlexAlignment::NONE:
        currentOffset = 0.0f;
        spacing = absPadding;
        break;
    case UiFlexAlignment::FILL:
        // negative when the children ask for more than there is, so they shrink instead of overlapping
        if (hasFlexItems) {
            extraItemSizes = s_shareFlexSpace(sortedChildren, mainSizes, remainingSpace, isVertical);
        } else if (validChildren > 0) {
            std::fill(extraItemSizes.begin(), extraItemSizes.end(), remainingSpace / static_cast<float>(validChildren));
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

        float mainSize = std::max(0.0f, (isVertical ? childSize.y : childSize.x) + extraItemSizes[idx]);
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
