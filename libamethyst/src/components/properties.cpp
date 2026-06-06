#include "components/properties.h"

namespace Amethyst {

#define AM_APPLY(field)                               \
    if (propIsSet(src.field) && field != src.field) { \
        field = src.field;                            \
        changed = true;                               \
    }

bool BaseProperties::apply(const BaseProperties &src)
{
    bool changed = false;
    AM_APPLY(active)
    AM_APPLY(anchorPoint)
    AM_APPLY(automaticSize)
    AM_APPLY(clipsDescendants)
    AM_APPLY(guiState)
    AM_APPLY(interactable)
    AM_APPLY(layoutOrder)
    AM_APPLY(padding)
    AM_APPLY(margin)
    AM_APPLY(position)
    AM_APPLY(size)
    AM_APPLY(rotation)
    AM_APPLY(visible)
    AM_APPLY(zIndex)
    AM_APPLY(zindexBehavior)
    return changed;
}

bool BaseStyleProperties::apply(const BaseStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(backgroundColor)
    AM_APPLY(backgroundTransparency)
    AM_APPLY(borderMode)
    AM_APPLY(borderPixelSize)
    AM_APPLY(borderColor)
    AM_APPLY(borderTransparency)
    AM_APPLY(cornerRadius)
    return changed;
}

bool TextStyleProperties::apply(const TextStyleProperties &src)
{
    bool changed = false;
    if (src.fontFamily.has_value() && fontFamily != src.fontFamily) {
        fontFamily = src.fontFamily;
        changed = true;
    }
    AM_APPLY(fontSize)
    AM_APPLY(textColor)
    AM_APPLY(textXAlignment)
    AM_APPLY(textYAlignment)
    AM_APPLY(textTruncate)
    AM_APPLY(richText)
    AM_APPLY(textWrapped)
    AM_APPLY(textScaled)
    AM_APPLY(lineHeight)
    AM_APPLY(strokeThickness)
    AM_APPLY(strokeColor)
    return changed;
}

bool ImageStyleProperties::apply(const ImageStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(imageColor)
    AM_APPLY(imageTransparency)
    AM_APPLY(scaleType)
    AM_APPLY(tileSize)
    return changed;
}

bool ButtonProperties::apply(const ButtonProperties &src)
{
    bool changed = false;
    AM_APPLY(autoButtonColor)
    AM_APPLY(modal)
    return changed;
}

bool ScrollingFrameStyleProperties::apply(const ScrollingFrameStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(scrollAxis)
    AM_APPLY(scrollBarVisibility)
    AM_APPLY(canvasSize)
    AM_APPLY(canvasPosition)
    AM_APPLY(scrollBarColor)
    AM_APPLY(scrollBarTransparency)
    AM_APPLY(scrollBarThickness)
    AM_APPLY(scrollBarThumbColor)
    AM_APPLY(scrollBarThumbTransparency)
    AM_APPLY(scrollSpeed)
    AM_APPLY(elasticScrolling)
    return changed;
}

bool CheckboxStyleProperties::apply(const CheckboxStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(checkColor)
    AM_APPLY(checkTransparency)
    AM_APPLY(checkboxSize)
    return changed;
}

bool CollapsibleHeaderStyleProperties::apply(const CollapsibleHeaderStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(expanded)
    AM_APPLY(headerHeight)
    AM_APPLY(headerColor)
    AM_APPLY(headerTransparency)
    AM_APPLY(headerCornerRadius)
    AM_APPLY(showIndicator)
    AM_APPLY(indicatorSize)
    AM_APPLY(indicatorPadding)
    AM_APPLY(indicatorColor)
    if (titleStyle.apply(src.titleStyle)) {
        changed = true;
    }
    return changed;
}

bool DropdownStyleProperties::apply(const DropdownStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(popupDirection)
    AM_APPLY(maxVisibleItems)
    AM_APPLY(itemHeight)
    AM_APPLY(popupWidth)
    AM_APPLY(itemFontSize)
    AM_APPLY(popupBackground)
    AM_APPLY(itemTextColor)
    AM_APPLY(itemDisabledColor)
    AM_APPLY(itemHoverBackground)
    AM_APPLY(separatorColor)
    return changed;
}

bool TabBarStyleProperties::apply(const TabBarStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(closeable)
    AM_APPLY(persistLayout)
    AM_APPLY(mode)
    AM_APPLY(tabPosition)
    AM_APPLY(visibility)
    AM_APPLY(barThickness)
    AM_APPLY(tabWidth)
    AM_APPLY(tabSpacing)
    AM_APPLY(tabOffset)
    AM_APPLY(tabColor)
    AM_APPLY(focussedTabColor)
    AM_APPLY(hoveredTabColor)
    AM_APPLY(pressedTabColor)
    AM_APPLY(closeButtonVisibility)
    AM_APPLY(tabTearOffEnabled)
    return changed;
}

bool MenuBarStyleProperties::apply(const MenuBarStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(entryPaddingX)
    AM_APPLY(entryPaddingY)
    AM_APPLY(entryFontSize)
    AM_APPLY(entryHoverBackground)
    AM_APPLY(entryActiveBackground)
    return changed;
}

bool TextInputStyleProperties::apply(const TextInputStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(placeholderColor)
    AM_APPLY(selectionColor)
    AM_APPLY(cursorColor)
    AM_APPLY(multiline)
    AM_APPLY(maxLength)
    AM_APPLY(readOnly)
    AM_APPLY(cursorBlinkRate)
    if (text.apply(src.text)) {
        changed = true;
    }
    return changed;
}

bool TableStyleProperties::apply(const TableStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(rowHeight)
    AM_APPLY(cellPadding)
    AM_APPLY(showColumnSeparators)
    AM_APPLY(columnSeparatorWidth)
    AM_APPLY(columnSeparatorColor)
    AM_APPLY(showHeader)
    AM_APPLY(headerHeight)
    AM_APPLY(headerColor)
    AM_APPLY(rowBackgroundColor)
    AM_APPLY(rowAlternateColor)
    AM_APPLY(rowHoverColor)
    AM_APPLY(rowSelectedColor)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

bool SliderStyleProperties::apply(const SliderStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(sliderColor)
    AM_APPLY(sliderTransparency)
    AM_APPLY(thumbColor)
    AM_APPLY(thumbTransparency)
    AM_APPLY(trackCornerRadius)
    AM_APPLY(thumbCornerRadius)
    AM_APPLY(labelColor)
    AM_APPLY(labelSide)
    AM_APPLY(labelPadding)
    AM_APPLY(valueColor)
    AM_APPLY(fontSize)
    AM_APPLY(layout)
    return changed;
}

bool TreeViewStyleProperties::apply(const TreeViewStyleProperties &src)
{
    bool changed = false;
    AM_APPLY(rowHeight)
    AM_APPLY(cellPadding)
    AM_APPLY(showColumnSeparators)
    AM_APPLY(columnSeparatorWidth)
    AM_APPLY(columnSeparatorColor)
    AM_APPLY(showDisclosureTriangles)
    AM_APPLY(disclosureTriangleSize)
    AM_APPLY(disclosureTrianglePadding)
    AM_APPLY(disclosureTriangleColor)
    AM_APPLY(indentPerLevel)
    AM_APPLY(rowBackgroundColor)
    AM_APPLY(rowAlternateColor)
    AM_APPLY(rowHoverColor)
    AM_APPLY(rowSelectedColor)
    AM_APPLY(fillRows)
    AM_APPLY(showHeader)
    AM_APPLY(headerHeight)
    AM_APPLY(headerColor)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

#undef AM_APPLY

} // namespace Amethyst
