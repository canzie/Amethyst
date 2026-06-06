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

#define AM_DIFF(field) \
    if (field != base.field) { \
        out.field = field; \
    }

BaseProperties BaseProperties::diff(const BaseProperties &base) const
{
    BaseProperties out;
    AM_DIFF(active)
    AM_DIFF(anchorPoint)
    AM_DIFF(automaticSize)
    AM_DIFF(clipsDescendants)
    AM_DIFF(guiState)
    AM_DIFF(interactable)
    AM_DIFF(layoutOrder)
    AM_DIFF(padding)
    AM_DIFF(margin)
    AM_DIFF(position)
    AM_DIFF(size)
    AM_DIFF(rotation)
    AM_DIFF(visible)
    AM_DIFF(zIndex)
    AM_DIFF(zindexBehavior)
    return out;
}

BaseStyleProperties BaseStyleProperties::diff(const BaseStyleProperties &base) const
{
    BaseStyleProperties out;
    AM_DIFF(backgroundColor)
    AM_DIFF(backgroundTransparency)
    AM_DIFF(borderMode)
    AM_DIFF(borderPixelSize)
    AM_DIFF(borderColor)
    AM_DIFF(borderTransparency)
    AM_DIFF(cornerRadius)
    return out;
}

TextStyleProperties TextStyleProperties::diff(const TextStyleProperties &base) const
{
    TextStyleProperties out;
    if (fontFamily != base.fontFamily) {
        out.fontFamily = fontFamily;
    }
    AM_DIFF(fontSize)
    AM_DIFF(textColor)
    AM_DIFF(textXAlignment)
    AM_DIFF(textYAlignment)
    AM_DIFF(textTruncate)
    AM_DIFF(richText)
    AM_DIFF(textWrapped)
    AM_DIFF(textScaled)
    AM_DIFF(lineHeight)
    AM_DIFF(strokeThickness)
    AM_DIFF(strokeColor)
    return out;
}

ImageStyleProperties ImageStyleProperties::diff(const ImageStyleProperties &base) const
{
    ImageStyleProperties out;
    AM_DIFF(imageColor)
    AM_DIFF(imageTransparency)
    AM_DIFF(scaleType)
    AM_DIFF(tileSize)
    return out;
}

ButtonProperties ButtonProperties::diff(const ButtonProperties &base) const
{
    ButtonProperties out;
    AM_DIFF(autoButtonColor)
    AM_DIFF(modal)
    return out;
}

ScrollingFrameStyleProperties ScrollingFrameStyleProperties::diff(const ScrollingFrameStyleProperties &base) const
{
    ScrollingFrameStyleProperties out;
    AM_DIFF(scrollAxis)
    AM_DIFF(scrollBarVisibility)
    AM_DIFF(canvasSize)
    AM_DIFF(canvasPosition)
    AM_DIFF(scrollBarColor)
    AM_DIFF(scrollBarTransparency)
    AM_DIFF(scrollBarThickness)
    AM_DIFF(scrollBarThumbColor)
    AM_DIFF(scrollBarThumbTransparency)
    AM_DIFF(scrollSpeed)
    AM_DIFF(elasticScrolling)
    return out;
}

CheckboxStyleProperties CheckboxStyleProperties::diff(const CheckboxStyleProperties &base) const
{
    CheckboxStyleProperties out;
    AM_DIFF(checkColor)
    AM_DIFF(checkTransparency)
    AM_DIFF(checkboxSize)
    return out;
}

CollapsibleHeaderStyleProperties CollapsibleHeaderStyleProperties::diff(const CollapsibleHeaderStyleProperties &base) const
{
    CollapsibleHeaderStyleProperties out;
    AM_DIFF(expanded)
    AM_DIFF(headerHeight)
    AM_DIFF(headerColor)
    AM_DIFF(headerTransparency)
    AM_DIFF(headerCornerRadius)
    AM_DIFF(showIndicator)
    AM_DIFF(indicatorSize)
    AM_DIFF(indicatorPadding)
    AM_DIFF(indicatorColor)
    out.titleStyle = titleStyle.diff(base.titleStyle);
    return out;
}

DropdownStyleProperties DropdownStyleProperties::diff(const DropdownStyleProperties &base) const
{
    DropdownStyleProperties out;
    AM_DIFF(popupDirection)
    AM_DIFF(maxVisibleItems)
    AM_DIFF(itemHeight)
    AM_DIFF(popupWidth)
    AM_DIFF(itemFontSize)
    AM_DIFF(popupBackground)
    AM_DIFF(itemTextColor)
    AM_DIFF(itemDisabledColor)
    AM_DIFF(itemHoverBackground)
    AM_DIFF(separatorColor)
    return out;
}

TabBarStyleProperties TabBarStyleProperties::diff(const TabBarStyleProperties &base) const
{
    TabBarStyleProperties out;
    AM_DIFF(closeable)
    AM_DIFF(persistLayout)
    AM_DIFF(mode)
    AM_DIFF(tabPosition)
    AM_DIFF(visibility)
    AM_DIFF(barThickness)
    AM_DIFF(tabWidth)
    AM_DIFF(tabSpacing)
    AM_DIFF(tabOffset)
    AM_DIFF(tabColor)
    AM_DIFF(focussedTabColor)
    AM_DIFF(hoveredTabColor)
    AM_DIFF(pressedTabColor)
    AM_DIFF(closeButtonVisibility)
    AM_DIFF(tabTearOffEnabled)
    return out;
}

MenuBarStyleProperties MenuBarStyleProperties::diff(const MenuBarStyleProperties &base) const
{
    MenuBarStyleProperties out;
    AM_DIFF(entryPaddingX)
    AM_DIFF(entryPaddingY)
    AM_DIFF(entryFontSize)
    AM_DIFF(entryHoverBackground)
    AM_DIFF(entryActiveBackground)
    return out;
}

TextInputStyleProperties TextInputStyleProperties::diff(const TextInputStyleProperties &base) const
{
    TextInputStyleProperties out;
    AM_DIFF(placeholderColor)
    AM_DIFF(selectionColor)
    AM_DIFF(cursorColor)
    AM_DIFF(multiline)
    AM_DIFF(maxLength)
    AM_DIFF(readOnly)
    AM_DIFF(cursorBlinkRate)
    out.text = text.diff(base.text);
    return out;
}

TableStyleProperties TableStyleProperties::diff(const TableStyleProperties &base) const
{
    TableStyleProperties out;
    AM_DIFF(rowHeight)
    AM_DIFF(cellPadding)
    AM_DIFF(showColumnSeparators)
    AM_DIFF(columnSeparatorWidth)
    AM_DIFF(columnSeparatorColor)
    AM_DIFF(showHeader)
    AM_DIFF(headerHeight)
    AM_DIFF(headerColor)
    AM_DIFF(rowBackgroundColor)
    AM_DIFF(rowAlternateColor)
    AM_DIFF(rowHoverColor)
    AM_DIFF(rowSelectedColor)
    out.header = header.diff(base.header);
    return out;
}

SliderStyleProperties SliderStyleProperties::diff(const SliderStyleProperties &base) const
{
    SliderStyleProperties out;
    AM_DIFF(sliderColor)
    AM_DIFF(sliderTransparency)
    AM_DIFF(thumbColor)
    AM_DIFF(thumbTransparency)
    AM_DIFF(trackCornerRadius)
    AM_DIFF(thumbCornerRadius)
    AM_DIFF(labelColor)
    AM_DIFF(labelSide)
    AM_DIFF(labelPadding)
    AM_DIFF(valueColor)
    AM_DIFF(fontSize)
    AM_DIFF(layout)
    return out;
}

TreeViewStyleProperties TreeViewStyleProperties::diff(const TreeViewStyleProperties &base) const
{
    TreeViewStyleProperties out;
    AM_DIFF(rowHeight)
    AM_DIFF(cellPadding)
    AM_DIFF(showColumnSeparators)
    AM_DIFF(columnSeparatorWidth)
    AM_DIFF(columnSeparatorColor)
    AM_DIFF(showDisclosureTriangles)
    AM_DIFF(disclosureTriangleSize)
    AM_DIFF(disclosureTrianglePadding)
    AM_DIFF(disclosureTriangleColor)
    AM_DIFF(indentPerLevel)
    AM_DIFF(rowBackgroundColor)
    AM_DIFF(rowAlternateColor)
    AM_DIFF(rowHoverColor)
    AM_DIFF(rowSelectedColor)
    AM_DIFF(fillRows)
    AM_DIFF(showHeader)
    AM_DIFF(headerHeight)
    AM_DIFF(headerColor)
    out.header = header.diff(base.header);
    return out;
}

#undef AM_DIFF

} // namespace Amethyst
