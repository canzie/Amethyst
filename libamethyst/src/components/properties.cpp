#include "components/properties.h"

namespace Amethyst {

#define AM_APPLY_ARGS(field)             \
    if (src.field.has_value()) {           \
        if (field != src.field.value()) {  \
            field = src.field.value();     \
            changed = true;                \
        }                                  \
    }

bool BaseProperties::apply(const BasePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(active)
    AM_APPLY_ARGS(anchorPoint)
    AM_APPLY_ARGS(automaticSize)
    AM_APPLY_ARGS(clipsDescendants)
    AM_APPLY_ARGS(interactable)
    AM_APPLY_ARGS(layoutOrder)
    AM_APPLY_ARGS(padding)
    AM_APPLY_ARGS(margin)
    AM_APPLY_ARGS(position)
    AM_APPLY_ARGS(size)
    AM_APPLY_ARGS(rotation)
    AM_APPLY_ARGS(visible)
    AM_APPLY_ARGS(zIndex)
    AM_APPLY_ARGS(zindexBehavior)
    return changed;
}

BaseProperties::operator BasePropertiesArgs() const
{
    BasePropertiesArgs out;
    out.active = active;
    out.anchorPoint = anchorPoint;
    out.automaticSize = automaticSize;
    out.clipsDescendants = clipsDescendants;
    out.interactable = interactable;
    out.layoutOrder = layoutOrder;
    out.padding = padding;
    out.margin = margin;
    out.position = position;
    out.size = size;
    out.rotation = rotation;
    out.visible = visible;
    out.zIndex = zIndex;
    out.zindexBehavior = zindexBehavior;
    return out;
}

bool ButtonProperties::apply(const ButtonPropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(autoButtonColor)
    AM_APPLY_ARGS(modal)
    return changed;
}

#undef AM_APPLY_ARGS

// Theme-resolve merge (Plain src): honors the mask, never writes it.
#define AM_APPLY_PLAIN(field)                      \
    if (!overridden.field && field != src.field) { \
        field = src.field;                         \
        changed = true;                            \
    }

// Instance override (Args src): writes the mask.
#define AM_APPLY_ARGS(field)              \
    if (src.field.has_value()) {          \
        if (field != src.field.value()) { \
            field = src.field.value();    \
            changed = true;               \
        }                                 \
        overridden.field = true;          \
    }

#define AM_TO_ARGS(field) out.field = field;

bool SplineStyleProperties::apply(const SplineStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(thickness)
    AM_APPLY_PLAIN(color)
    AM_APPLY_PLAIN(knotSize)
    return changed;
}

bool SplineStyleProperties::apply(const SplineStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(type)
    AM_APPLY_ARGS(showKnots)
    AM_APPLY_ARGS(thickness)
    AM_APPLY_ARGS(color)
    AM_APPLY_ARGS(knotSize)
    return changed;
}

SplineStyleProperties::operator SplineStylePropertiesArgs() const
{
    SplineStylePropertiesArgs out;
    AM_TO_ARGS(type)
    AM_TO_ARGS(showKnots)
    AM_TO_ARGS(thickness)
    AM_TO_ARGS(color)
    AM_TO_ARGS(knotSize)
    return out;
}

bool ContextMenuStyleProperties::apply(const ContextMenuStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(itemHoverBackground)
    AM_APPLY_PLAIN(separatorColor)
    return changed;
}

bool ContextMenuStyleProperties::apply(const ContextMenuStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(itemHoverBackground)
    AM_APPLY_ARGS(separatorColor)
    return changed;
}

ContextMenuStyleProperties::operator ContextMenuStylePropertiesArgs() const
{
    ContextMenuStylePropertiesArgs out;
    AM_TO_ARGS(itemHoverBackground)
    AM_TO_ARGS(separatorColor)
    return out;
}

bool DropdownStyleProperties::apply(const DropdownStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(itemFontSize)
    AM_APPLY_PLAIN(popupBackground)
    AM_APPLY_PLAIN(itemTextColor)
    AM_APPLY_PLAIN(itemDisabledColor)
    AM_APPLY_PLAIN(itemHoverBackground)
    AM_APPLY_PLAIN(separatorColor)
    return changed;
}

bool DropdownStyleProperties::apply(const DropdownStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(popupDirection)
    AM_APPLY_ARGS(maxVisibleItems)
    AM_APPLY_ARGS(itemHeight)
    AM_APPLY_ARGS(popupWidth)
    AM_APPLY_ARGS(itemFontSize)
    AM_APPLY_ARGS(popupBackground)
    AM_APPLY_ARGS(itemTextColor)
    AM_APPLY_ARGS(itemDisabledColor)
    AM_APPLY_ARGS(itemHoverBackground)
    AM_APPLY_ARGS(separatorColor)
    return changed;
}

DropdownStyleProperties::operator DropdownStylePropertiesArgs() const
{
    DropdownStylePropertiesArgs out;
    AM_TO_ARGS(popupDirection)
    AM_TO_ARGS(maxVisibleItems)
    AM_TO_ARGS(itemHeight)
    AM_TO_ARGS(popupWidth)
    AM_TO_ARGS(itemFontSize)
    AM_TO_ARGS(popupBackground)
    AM_TO_ARGS(itemTextColor)
    AM_TO_ARGS(itemDisabledColor)
    AM_TO_ARGS(itemHoverBackground)
    AM_TO_ARGS(separatorColor)
    return out;
}

bool BaseStyleProperties::apply(const BaseStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(backgroundColor)
    AM_APPLY_PLAIN(backgroundTransparency)
    AM_APPLY_PLAIN(borderMode)
    AM_APPLY_PLAIN(borderPixelSize)
    AM_APPLY_PLAIN(borderColor)
    AM_APPLY_PLAIN(borderTransparency)
    AM_APPLY_PLAIN(cornerRadius)
    return changed;
}

bool BaseStyleProperties::apply(const BaseStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(backgroundColor)
    AM_APPLY_ARGS(backgroundTransparency)
    AM_APPLY_ARGS(borderMode)
    AM_APPLY_ARGS(borderPixelSize)
    AM_APPLY_ARGS(borderColor)
    AM_APPLY_ARGS(borderTransparency)
    AM_APPLY_ARGS(cornerRadius)
    return changed;
}

BaseStyleProperties::operator BaseStylePropertiesArgs() const
{
    BaseStylePropertiesArgs out;
    AM_TO_ARGS(backgroundColor)
    AM_TO_ARGS(backgroundTransparency)
    AM_TO_ARGS(borderMode)
    AM_TO_ARGS(borderPixelSize)
    AM_TO_ARGS(borderColor)
    AM_TO_ARGS(borderTransparency)
    AM_TO_ARGS(cornerRadius)
    return out;
}

bool TextStyleProperties::apply(const TextStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(fontSize)
    AM_APPLY_PLAIN(textColor)
    AM_APPLY_PLAIN(textXAlignment)
    AM_APPLY_PLAIN(textYAlignment)
    AM_APPLY_PLAIN(lineHeight)
    AM_APPLY_PLAIN(strokeThickness)
    AM_APPLY_PLAIN(strokeColor)
    AM_APPLY_PLAIN(fontFamily)
    return changed;
}

bool TextStyleProperties::apply(const TextStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(fontSize)
    AM_APPLY_ARGS(textColor)
    AM_APPLY_ARGS(textXAlignment)
    AM_APPLY_ARGS(textYAlignment)
    AM_APPLY_ARGS(lineHeight)
    AM_APPLY_ARGS(strokeThickness)
    AM_APPLY_ARGS(strokeColor)
    AM_APPLY_ARGS(textTruncate)
    AM_APPLY_ARGS(richText)
    AM_APPLY_ARGS(textWrapped)
    AM_APPLY_ARGS(textScaled)
    AM_APPLY_ARGS(fontFamily)
    return changed;
}

TextStyleProperties::operator TextStylePropertiesArgs() const
{
    TextStylePropertiesArgs out;
    AM_TO_ARGS(fontSize)
    AM_TO_ARGS(textColor)
    AM_TO_ARGS(textXAlignment)
    AM_TO_ARGS(textYAlignment)
    AM_TO_ARGS(lineHeight)
    AM_TO_ARGS(strokeThickness)
    AM_TO_ARGS(strokeColor)
    AM_TO_ARGS(textTruncate)
    AM_TO_ARGS(richText)
    AM_TO_ARGS(textWrapped)
    AM_TO_ARGS(textScaled)
    AM_TO_ARGS(fontFamily)
    return out;
}

bool ImageStyleProperties::apply(const ImageStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(imageColor)
    AM_APPLY_PLAIN(scaleType)
    AM_APPLY_PLAIN(tileSize)
    return changed;
}

bool ImageStyleProperties::apply(const ImageStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(imageColor)
    AM_APPLY_ARGS(scaleType)
    AM_APPLY_ARGS(tileSize)
    return changed;
}

ImageStyleProperties::operator ImageStylePropertiesArgs() const
{
    ImageStylePropertiesArgs out;
    AM_TO_ARGS(imageColor)
    AM_TO_ARGS(scaleType)
    AM_TO_ARGS(tileSize)
    return out;
}

bool ScrollingFrameStyleProperties::apply(const ScrollingFrameStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(scrollBarColor)
    AM_APPLY_PLAIN(scrollBarTransparency)
    AM_APPLY_PLAIN(scrollBarThickness)
    AM_APPLY_PLAIN(scrollBarThumbColor)
    AM_APPLY_PLAIN(scrollBarThumbTransparency)
    return changed;
}

bool ScrollingFrameStyleProperties::apply(const ScrollingFrameStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(scrollAxis)
    AM_APPLY_ARGS(scrollBarVisibility)
    AM_APPLY_ARGS(canvasSize)
    AM_APPLY_ARGS(canvasPosition)
    AM_APPLY_ARGS(automaticCanvasSize)
    AM_APPLY_ARGS(scrollSpeed)
    AM_APPLY_ARGS(elasticScrolling)
    AM_APPLY_ARGS(scrollBarColor)
    AM_APPLY_ARGS(scrollBarTransparency)
    AM_APPLY_ARGS(scrollBarThickness)
    AM_APPLY_ARGS(scrollBarThumbColor)
    AM_APPLY_ARGS(scrollBarThumbTransparency)
    return changed;
}

ScrollingFrameStyleProperties::operator ScrollingFrameStylePropertiesArgs() const
{
    ScrollingFrameStylePropertiesArgs out;
    AM_TO_ARGS(scrollAxis)
    AM_TO_ARGS(scrollBarVisibility)
    AM_TO_ARGS(canvasSize)
    AM_TO_ARGS(canvasPosition)
    AM_TO_ARGS(automaticCanvasSize)
    AM_TO_ARGS(scrollSpeed)
    AM_TO_ARGS(elasticScrolling)
    AM_TO_ARGS(scrollBarColor)
    AM_TO_ARGS(scrollBarTransparency)
    AM_TO_ARGS(scrollBarThickness)
    AM_TO_ARGS(scrollBarThumbColor)
    AM_TO_ARGS(scrollBarThumbTransparency)
    return out;
}

bool CheckboxStyleProperties::apply(const CheckboxStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(checkColor)
    return changed;
}

bool CheckboxStyleProperties::apply(const CheckboxStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(checkColor)
    return changed;
}

CheckboxStyleProperties::operator CheckboxStylePropertiesArgs() const
{
    CheckboxStylePropertiesArgs out;
    AM_TO_ARGS(checkColor)
    return out;
}

bool CollapsibleHeaderStyleProperties::apply(const CollapsibleHeaderStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(indicatorSize)
    AM_APPLY_PLAIN(indicatorPadding)
    AM_APPLY_PLAIN(indicatorColor)
    if (titleStyle.apply(src.titleStyle)) {
        changed = true;
    }
    return changed;
}

bool CollapsibleHeaderStyleProperties::apply(const CollapsibleHeaderStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(expanded)
    AM_APPLY_ARGS(headerHeight)
    AM_APPLY_ARGS(showIndicator)
    AM_APPLY_ARGS(indicatorSize)
    AM_APPLY_ARGS(indicatorPadding)
    AM_APPLY_ARGS(indicatorColor)
    if (titleStyle.apply(src.titleStyle)) {
        changed = true;
    }
    return changed;
}

CollapsibleHeaderStyleProperties::operator CollapsibleHeaderStylePropertiesArgs() const
{
    CollapsibleHeaderStylePropertiesArgs out;
    AM_TO_ARGS(expanded)
    out.titleStyle = titleStyle;
    AM_TO_ARGS(headerHeight)
    AM_TO_ARGS(showIndicator)
    AM_TO_ARGS(indicatorSize)
    AM_TO_ARGS(indicatorPadding)
    AM_TO_ARGS(indicatorColor)
    return out;
}

bool TabBarStyleProperties::apply(const TabBarStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(barThickness)
    AM_APPLY_PLAIN(tabWidth)
    AM_APPLY_PLAIN(tabSpacing)
    AM_APPLY_PLAIN(tabOffset)
    AM_APPLY_PLAIN(tabCornerRadius)
    AM_APPLY_PLAIN(closeColor)
    return changed;
}

bool TabBarStyleProperties::apply(const TabBarStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(closeable)
    AM_APPLY_ARGS(persistLayout)
    AM_APPLY_ARGS(mode)
    AM_APPLY_ARGS(tabPosition)
    AM_APPLY_ARGS(visibility)
    AM_APPLY_ARGS(barThickness)
    AM_APPLY_ARGS(tabWidth)
    AM_APPLY_ARGS(tabSpacing)
    AM_APPLY_ARGS(tabOffset)
    AM_APPLY_ARGS(tabCornerRadius)
    AM_APPLY_ARGS(closeColor)
    AM_APPLY_ARGS(closeButtonVisibility)
    AM_APPLY_ARGS(tabTearOffEnabled)
    return changed;
}

TabBarStyleProperties::operator TabBarStylePropertiesArgs() const
{
    TabBarStylePropertiesArgs out;
    AM_TO_ARGS(closeable)
    AM_TO_ARGS(persistLayout)
    AM_TO_ARGS(mode)
    AM_TO_ARGS(tabPosition)
    AM_TO_ARGS(visibility)
    AM_TO_ARGS(barThickness)
    AM_TO_ARGS(tabWidth)
    AM_TO_ARGS(tabSpacing)
    AM_TO_ARGS(tabOffset)
    AM_TO_ARGS(tabCornerRadius)
    AM_TO_ARGS(closeColor)
    AM_TO_ARGS(closeButtonVisibility)
    AM_TO_ARGS(tabTearOffEnabled)
    return out;
}

bool MenuBarStyleProperties::apply(const MenuBarStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(entryPaddingX)
    AM_APPLY_PLAIN(entryPaddingY)
    AM_APPLY_PLAIN(entryFontSize)
    return changed;
}

bool MenuBarStyleProperties::apply(const MenuBarStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(entryPaddingX)
    AM_APPLY_ARGS(entryPaddingY)
    AM_APPLY_ARGS(entryFontSize)
    return changed;
}

MenuBarStyleProperties::operator MenuBarStylePropertiesArgs() const
{
    MenuBarStylePropertiesArgs out;
    AM_TO_ARGS(entryPaddingX)
    AM_TO_ARGS(entryPaddingY)
    AM_TO_ARGS(entryFontSize)
    return out;
}

bool TextInputStyleProperties::apply(const TextInputStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(placeholderColor)
    AM_APPLY_PLAIN(selectionColor)
    AM_APPLY_PLAIN(cursorColor)
    AM_APPLY_PLAIN(cursorBlinkRate)
    if (text.apply(src.text)) {
        changed = true;
    }
    return changed;
}

bool TextInputStyleProperties::apply(const TextInputStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(multiline)
    AM_APPLY_ARGS(maxLength)
    AM_APPLY_ARGS(readOnly)
    AM_APPLY_ARGS(placeholderColor)
    AM_APPLY_ARGS(selectionColor)
    AM_APPLY_ARGS(cursorColor)
    AM_APPLY_ARGS(cursorBlinkRate)
    if (text.apply(src.text)) {
        changed = true;
    }
    return changed;
}

TextInputStyleProperties::operator TextInputStylePropertiesArgs() const
{
    TextInputStylePropertiesArgs out;
    out.text = text;
    AM_TO_ARGS(multiline)
    AM_TO_ARGS(maxLength)
    AM_TO_ARGS(readOnly)
    AM_TO_ARGS(placeholderColor)
    AM_TO_ARGS(selectionColor)
    AM_TO_ARGS(cursorColor)
    AM_TO_ARGS(cursorBlinkRate)
    return out;
}

bool TableStyleProperties::apply(const TableStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(rowHeight)
    AM_APPLY_PLAIN(separatorWidth)
    AM_APPLY_PLAIN(separatorColor)
    AM_APPLY_PLAIN(headerHeight)
    AM_APPLY_PLAIN(headerColor)
    AM_APPLY_PLAIN(rowBackgroundColor)
    AM_APPLY_PLAIN(rowAlternateColor)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

bool TableStyleProperties::apply(const TableStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(cellPadding)
    AM_APPLY_ARGS(separatorMode)
    AM_APPLY_ARGS(showHeader)
    AM_APPLY_ARGS(selectedRowColor)
    AM_APPLY_ARGS(rowHeight)
    AM_APPLY_ARGS(separatorWidth)
    AM_APPLY_ARGS(separatorColor)
    AM_APPLY_ARGS(headerHeight)
    AM_APPLY_ARGS(headerColor)
    AM_APPLY_ARGS(rowBackgroundColor)
    AM_APPLY_ARGS(rowAlternateColor)
    AM_APPLY_ARGS(scrollBarVisibility)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

TableStyleProperties::operator TableStylePropertiesArgs() const
{
    TableStylePropertiesArgs out;
    AM_TO_ARGS(cellPadding)
    AM_TO_ARGS(separatorMode)
    AM_TO_ARGS(showHeader)
    AM_TO_ARGS(selectedRowColor)
    out.header = header;
    AM_TO_ARGS(rowHeight)
    AM_TO_ARGS(separatorWidth)
    AM_TO_ARGS(separatorColor)
    AM_TO_ARGS(headerHeight)
    AM_TO_ARGS(headerColor)
    AM_TO_ARGS(rowBackgroundColor)
    AM_TO_ARGS(rowAlternateColor)
    AM_TO_ARGS(scrollBarVisibility)
    return out;
}

bool SliderStyleProperties::apply(const SliderStyleProperties &src)
{
    bool changed = false;
    if (thumb.apply(src.thumb)) {
        changed = true;
    }
    if (text.apply(src.text)) {
        changed = true;
    }
    AM_APPLY_PLAIN(trackHeight)
    AM_APPLY_PLAIN(thumbWidth)
    AM_APPLY_PLAIN(thumbHeight)
    AM_APPLY_PLAIN(fillColor)
    AM_APPLY_PLAIN(labelPadding)
    return changed;
}

bool SliderStyleProperties::apply(const SliderStylePropertiesArgs &src)
{
    bool changed = false;
    if (thumb.apply(src.thumb)) {
        changed = true;
    }
    if (text.apply(src.text)) {
        changed = true;
    }
    AM_APPLY_ARGS(trackHeight)
    AM_APPLY_ARGS(thumbWidth)
    AM_APPLY_ARGS(thumbHeight)
    AM_APPLY_ARGS(fillColor)
    AM_APPLY_ARGS(labelPadding)
    return changed;
}

SliderStyleProperties::operator SliderStylePropertiesArgs() const
{
    SliderStylePropertiesArgs out;
    out.thumb = thumb;
    out.text = text;
    AM_TO_ARGS(trackHeight)
    AM_TO_ARGS(thumbWidth)
    AM_TO_ARGS(thumbHeight)
    AM_TO_ARGS(fillColor)
    AM_TO_ARGS(labelPadding)
    return out;
}

bool DragStyleProperties::apply(const DragStyleProperties &src)
{
    bool changed = false;
    if (text.apply(src.text)) {
        changed = true;
    }
    return changed;
}

bool DragStyleProperties::apply(const DragStylePropertiesArgs &src)
{
    bool changed = false;
    if (text.apply(src.text)) {
        changed = true;
    }
    return changed;
}

DragStyleProperties::operator DragStylePropertiesArgs() const
{
    DragStylePropertiesArgs out;
    out.text = text;
    return out;
}

bool TreeViewStyleProperties::apply(const TreeViewStyleProperties &src)
{
    bool changed = false;
    AM_APPLY_PLAIN(rowHeight)
    AM_APPLY_PLAIN(disclosureTriangleSize)
    AM_APPLY_PLAIN(disclosureTrianglePadding)
    AM_APPLY_PLAIN(disclosureTriangleColor)
    AM_APPLY_PLAIN(indentPerLevel)
    AM_APPLY_PLAIN(rowBackgroundColor)
    AM_APPLY_PLAIN(rowAlternateColor)
    AM_APPLY_PLAIN(rowHoverColor)
    AM_APPLY_PLAIN(rowSelectedColor)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

bool TreeViewStyleProperties::apply(const TreeViewStylePropertiesArgs &src)
{
    bool changed = false;
    AM_APPLY_ARGS(cellPadding)
    AM_APPLY_ARGS(showColumnSeparators)
    AM_APPLY_ARGS(columnSeparatorWidth)
    AM_APPLY_ARGS(columnSeparatorColor)
    AM_APPLY_ARGS(showDisclosureTriangles)
    AM_APPLY_ARGS(fillRows)
    AM_APPLY_ARGS(showHeader)
    AM_APPLY_ARGS(headerHeight)
    AM_APPLY_ARGS(headerColor)
    AM_APPLY_ARGS(rowHeight)
    AM_APPLY_ARGS(disclosureTriangleSize)
    AM_APPLY_ARGS(disclosureTrianglePadding)
    AM_APPLY_ARGS(disclosureTriangleColor)
    AM_APPLY_ARGS(indentPerLevel)
    AM_APPLY_ARGS(rowBackgroundColor)
    AM_APPLY_ARGS(rowAlternateColor)
    AM_APPLY_ARGS(rowHoverColor)
    AM_APPLY_ARGS(rowSelectedColor)
    AM_APPLY_ARGS(scrollBarVisibility)
    if (header.apply(src.header)) {
        changed = true;
    }
    return changed;
}

TreeViewStyleProperties::operator TreeViewStylePropertiesArgs() const
{
    TreeViewStylePropertiesArgs out;
    AM_TO_ARGS(cellPadding)
    AM_TO_ARGS(showColumnSeparators)
    AM_TO_ARGS(columnSeparatorWidth)
    AM_TO_ARGS(columnSeparatorColor)
    AM_TO_ARGS(showDisclosureTriangles)
    AM_TO_ARGS(fillRows)
    AM_TO_ARGS(showHeader)
    AM_TO_ARGS(headerHeight)
    AM_TO_ARGS(headerColor)
    out.header = header;
    AM_TO_ARGS(rowHeight)
    AM_TO_ARGS(disclosureTriangleSize)
    AM_TO_ARGS(disclosureTrianglePadding)
    AM_TO_ARGS(disclosureTriangleColor)
    AM_TO_ARGS(indentPerLevel)
    AM_TO_ARGS(rowBackgroundColor)
    AM_TO_ARGS(rowAlternateColor)
    AM_TO_ARGS(rowHoverColor)
    AM_TO_ARGS(rowSelectedColor)
    AM_TO_ARGS(scrollBarVisibility)
    return out;
}

#undef AM_APPLY_PLAIN
#undef AM_APPLY_ARGS
#undef AM_TO_ARGS

} // namespace Amethyst
