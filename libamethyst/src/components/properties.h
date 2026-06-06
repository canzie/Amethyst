#ifndef AMETHYST__PROPERTIES_H
#define AMETHYST__PROPERTIES_H

#include "components/common.h"

#include <cmath>
#include <cstdint>
#include <glm/vec2.hpp>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Amethyst {

constexpr float PROP_UNSET_FLOAT = std::numeric_limits<float>::quiet_NaN();
constexpr int32_t PROP_UNSET_INT32 = INT32_MIN;
constexpr uint32_t PROP_UNSET_UINT32 = UINT32_MAX;
constexpr am_bool PROP_UNSET_BOOL = -1;

inline bool propIsSet(float v)
{
    return !std::isnan(v);
}
inline bool propIsSet(int32_t v)
{
    return v != PROP_UNSET_INT32;
}
inline bool propIsSet(uint32_t v)
{
    return v != PROP_UNSET_UINT32;
}
inline bool propIsSet(am_bool v)
{
    return v != PROP_UNSET_BOOL;
}
inline bool propIsSet(const glm::vec2 &v)
{
    return !std::isnan(v.x);
}
inline bool propIsSet(const Color3 &v)
{
    return !std::isnan(v.r);
}
inline bool propIsSet(const UDim &v)
{
    return !std::isnan(v.scale);
}
inline bool propIsSet(const UDim2 &v)
{
    return !std::isnan(v.scale.x);
}
inline bool propIsSet(const UDim4 &v)
{
    return !std::isnan(v.top.scale);
}
inline bool propIsSet(AutomaticSize v)
{
    return v != AutomaticSize::NONE;
}
inline bool propIsSet(BorderMode v)
{
    return v != BorderMode::NONE;
}
inline bool propIsSet(GuiState v)
{
    return v != GuiState::NONE;
}
inline bool propIsSet(ZIndexBehavior v)
{
    return v != ZIndexBehavior::NONE;
}
inline bool propIsSet(const Color4 &v)
{
    return !std::isnan(v.r);
}
inline bool propIsSet(TextXAlignment v)
{
    return v != TextXAlignment::NONE;
}
inline bool propIsSet(TextYAlignment v)
{
    return v != TextYAlignment::NONE;
}
inline bool propIsSet(TextTruncate v)
{
    return v != TextTruncate::NONE;
}
inline bool propIsSet(ImageScaleType v)
{
    return v != ImageScaleType::NONE;
}
inline bool propIsSet(LabelSide v)
{
    return v != LabelSide::NONE;
}
inline bool propIsSet(ValueControlLayout v)
{
    return v != ValueControlLayout::NONE;
}
inline bool propIsSet(ScrollBarVisibility v)
{
    return v != ScrollBarVisibility::NONE;
}
inline bool propIsSet(ScrollAxis v)
{
    return v != ScrollAxis::NONE;
}
inline bool propIsSet(DropdownDirection v)
{
    return v != DropdownDirection::NONE;
}
inline bool propIsSet(TabBarMode v)
{
    return v != TabBarMode::NONE;
}
inline bool propIsSet(TabBarVisibility v)
{
    return v != TabBarVisibility::NONE;
}
inline bool propIsSet(TabCloseButtonVisibility v)
{
    return v != TabCloseButtonVisibility::NONE;
}
inline bool propIsSet(TabBarPosition v)
{
    return v != TabBarPosition::NONE;
}
inline bool propIsSet(DragMode v)
{
    return v != DragMode::NONE;
}

struct BaseProperties {
    am_bool active = PROP_UNSET_BOOL;
    glm::vec2 anchorPoint = glm::vec2(PROP_UNSET_FLOAT);
    AutomaticSize automaticSize = AutomaticSize::NONE;
    am_bool clipsDescendants = PROP_UNSET_BOOL;
    GuiState guiState = GuiState::NONE;
    am_bool interactable = PROP_UNSET_BOOL;
    LayoutOrder layoutOrder = PROP_UNSET_UINT32;
    UDim4 padding = {{PROP_UNSET_FLOAT, 0}, {}, {}, {}};
    UDim4 margin = {{PROP_UNSET_FLOAT, 0}, {}, {}, {}};
    UDim2 position = UDim2(glm::vec2(PROP_UNSET_FLOAT), glm::vec2(0));
    UDim2 size = UDim2(glm::vec2(PROP_UNSET_FLOAT), glm::vec2(0));
    Degrees rotation = PROP_UNSET_FLOAT;
    am_bool visible = PROP_UNSET_BOOL;
    int32_t zIndex = PROP_UNSET_INT32;
    ZIndexBehavior zindexBehavior = ZIndexBehavior::NONE;

    bool apply(const BaseProperties &src);
    BaseProperties diff(const BaseProperties &base) const;
};

struct BaseStyleProperties {
    Color3 backgroundColor = Color3(PROP_UNSET_FLOAT);
    float backgroundTransparency = PROP_UNSET_FLOAT;
    BorderMode borderMode = BorderMode::NONE;
    float borderPixelSize = PROP_UNSET_FLOAT;
    Color3 borderColor = Color3(PROP_UNSET_FLOAT);
    float borderTransparency = PROP_UNSET_FLOAT;
    float cornerRadius = PROP_UNSET_FLOAT;

    bool apply(const BaseStyleProperties &src);
    BaseStyleProperties diff(const BaseStyleProperties &base) const;
};

struct TextStyleProperties {
    float fontSize = PROP_UNSET_FLOAT;
    Color4 textColor = Color4(PROP_UNSET_FLOAT);
    TextXAlignment textXAlignment = TextXAlignment::NONE;
    TextYAlignment textYAlignment = TextYAlignment::NONE;
    TextTruncate textTruncate = TextTruncate::NONE;
    am_bool richText = PROP_UNSET_BOOL;
    am_bool textWrapped = PROP_UNSET_BOOL;
    am_bool textScaled = PROP_UNSET_BOOL;
    float lineHeight = PROP_UNSET_FLOAT;
    float strokeThickness = PROP_UNSET_FLOAT;
    Color4 strokeColor = Color4(PROP_UNSET_FLOAT);
    std::optional<std::string> fontFamily{};

    bool apply(const TextStyleProperties &src);
    TextStyleProperties diff(const TextStyleProperties &base) const;
};

struct ImageStyleProperties {
    Color4 imageColor = Color4(PROP_UNSET_FLOAT);
    float imageTransparency = PROP_UNSET_FLOAT;
    ImageScaleType scaleType = ImageScaleType::NONE;
    glm::vec2 tileSize = glm::vec2(PROP_UNSET_FLOAT);

    bool apply(const ImageStyleProperties &src);
    ImageStyleProperties diff(const ImageStyleProperties &base) const;
};

struct ButtonProperties {
    am_bool autoButtonColor = PROP_UNSET_BOOL;
    am_bool modal = PROP_UNSET_BOOL;

    bool apply(const ButtonProperties &src);
    ButtonProperties diff(const ButtonProperties &base) const;
};

struct ScrollingFrameStyleProperties {
    ScrollAxis scrollAxis = ScrollAxis::NONE;
    ScrollBarVisibility scrollBarVisibility = ScrollBarVisibility::NONE;
    UDim2 canvasSize = UDim2(glm::vec2(PROP_UNSET_FLOAT), glm::vec2(0));
    UDim2 canvasPosition = UDim2(glm::vec2(PROP_UNSET_FLOAT), glm::vec2(0));
    Color3 scrollBarColor = Color3(PROP_UNSET_FLOAT);
    float scrollBarTransparency = PROP_UNSET_FLOAT;
    float scrollBarThickness = PROP_UNSET_FLOAT;
    Color3 scrollBarThumbColor = Color3(PROP_UNSET_FLOAT);
    float scrollBarThumbTransparency = PROP_UNSET_FLOAT;
    float scrollSpeed = PROP_UNSET_FLOAT;
    am_bool elasticScrolling = PROP_UNSET_BOOL;

    bool apply(const ScrollingFrameStyleProperties &src);
    ScrollingFrameStyleProperties diff(const ScrollingFrameStyleProperties &base) const;
};

struct CheckboxStyleProperties {
    Color3 checkColor = Color3(PROP_UNSET_FLOAT);
    float checkTransparency = PROP_UNSET_FLOAT;
    float checkboxSize = PROP_UNSET_FLOAT;

    bool apply(const CheckboxStyleProperties &src);
    CheckboxStyleProperties diff(const CheckboxStyleProperties &base) const;
};

struct CollapsibleHeaderStyleProperties {
    am_bool expanded = PROP_UNSET_BOOL;
    TextStyleProperties titleStyle{};
    float headerHeight = PROP_UNSET_FLOAT;
    Color3 headerColor = Color3(PROP_UNSET_FLOAT);
    float headerTransparency = PROP_UNSET_FLOAT;
    float headerCornerRadius = PROP_UNSET_FLOAT;
    am_bool showIndicator = PROP_UNSET_BOOL;
    float indicatorSize = PROP_UNSET_FLOAT;
    float indicatorPadding = PROP_UNSET_FLOAT;
    Color4 indicatorColor = Color4(PROP_UNSET_FLOAT);

    bool apply(const CollapsibleHeaderStyleProperties &src);
    CollapsibleHeaderStyleProperties diff(const CollapsibleHeaderStyleProperties &base) const;
};

struct DropdownStyleProperties {
    DropdownDirection popupDirection = DropdownDirection::NONE;
    int32_t maxVisibleItems = PROP_UNSET_INT32;
    float itemHeight = PROP_UNSET_FLOAT;
    float popupWidth = PROP_UNSET_FLOAT;
    float itemFontSize = PROP_UNSET_FLOAT;
    Color3 popupBackground = Color3(PROP_UNSET_FLOAT);
    Color4 itemTextColor = Color4(PROP_UNSET_FLOAT);
    Color4 itemDisabledColor = Color4(PROP_UNSET_FLOAT);
    Color3 itemHoverBackground = Color3(PROP_UNSET_FLOAT);
    Color3 separatorColor = Color3(PROP_UNSET_FLOAT);

    bool apply(const DropdownStyleProperties &src);
    DropdownStyleProperties diff(const DropdownStyleProperties &base) const;
};

struct TabBarStyleProperties {
    am_bool closeable = PROP_UNSET_BOOL;
    am_bool persistLayout = PROP_UNSET_BOOL;
    TabBarMode mode = TabBarMode::NONE;
    TabBarPosition tabPosition = TabBarPosition::NONE;
    TabBarVisibility visibility = TabBarVisibility::NONE;
    float barThickness = PROP_UNSET_FLOAT;
    float tabWidth = PROP_UNSET_FLOAT;
    float tabSpacing = PROP_UNSET_FLOAT;
    float tabOffset = PROP_UNSET_FLOAT;
    Color3 tabColor = Color3(PROP_UNSET_FLOAT);
    Color3 focussedTabColor = Color3(PROP_UNSET_FLOAT);
    Color3 hoveredTabColor = Color3(PROP_UNSET_FLOAT);
    Color3 pressedTabColor = Color3(PROP_UNSET_FLOAT);
    TabCloseButtonVisibility closeButtonVisibility = TabCloseButtonVisibility::NONE;
    am_bool tabTearOffEnabled = PROP_UNSET_BOOL;

    bool apply(const TabBarStyleProperties &src);
    TabBarStyleProperties diff(const TabBarStyleProperties &base) const;
};

struct MenuBarStyleProperties {
    float entryPaddingX = PROP_UNSET_FLOAT;
    float entryPaddingY = PROP_UNSET_FLOAT;
    float entryFontSize = PROP_UNSET_FLOAT;
    Color3 entryHoverBackground = Color3(PROP_UNSET_FLOAT);
    Color3 entryActiveBackground = Color3(PROP_UNSET_FLOAT);

    bool apply(const MenuBarStyleProperties &src);
    MenuBarStyleProperties diff(const MenuBarStyleProperties &base) const;
};

struct TextInputStyleProperties {
    TextStyleProperties text{};
    Color4 placeholderColor = Color4(PROP_UNSET_FLOAT);
    Color4 selectionColor = Color4(PROP_UNSET_FLOAT);
    Color4 cursorColor = Color4(PROP_UNSET_FLOAT);
    am_bool multiline = PROP_UNSET_BOOL;
    int32_t maxLength = PROP_UNSET_INT32;
    am_bool readOnly = PROP_UNSET_BOOL;
    float cursorBlinkRate = PROP_UNSET_FLOAT;

    bool apply(const TextInputStyleProperties &src);
    TextInputStyleProperties diff(const TextInputStyleProperties &base) const;
};

struct TableStyleProperties {
    float rowHeight = PROP_UNSET_FLOAT;
    UDim4 cellPadding = {{PROP_UNSET_FLOAT, 0}, {}, {}, {}};
    am_bool showColumnSeparators = PROP_UNSET_BOOL;
    float columnSeparatorWidth = PROP_UNSET_FLOAT;
    Color4 columnSeparatorColor = Color4(PROP_UNSET_FLOAT);
    am_bool showHeader = PROP_UNSET_BOOL;
    float headerHeight = PROP_UNSET_FLOAT;
    Color3 headerColor = Color3(PROP_UNSET_FLOAT);
    TextStyleProperties header{};
    Color4 rowBackgroundColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowAlternateColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowHoverColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowSelectedColor = Color4(PROP_UNSET_FLOAT);

    bool apply(const TableStyleProperties &src);
    TableStyleProperties diff(const TableStyleProperties &base) const;
};

struct SliderStyleProperties {
    Color3 sliderColor = Color3(PROP_UNSET_FLOAT);
    float sliderTransparency = PROP_UNSET_FLOAT;
    Color3 thumbColor = Color3(PROP_UNSET_FLOAT);
    float thumbTransparency = PROP_UNSET_FLOAT;
    float trackCornerRadius = PROP_UNSET_FLOAT;
    float thumbCornerRadius = PROP_UNSET_FLOAT;
    Color4 labelColor = Color4(PROP_UNSET_FLOAT);
    LabelSide labelSide = LabelSide::NONE;
    UDim labelPadding = {PROP_UNSET_FLOAT, 0};
    Color4 valueColor = Color4(PROP_UNSET_FLOAT);
    float fontSize = PROP_UNSET_FLOAT;
    ValueControlLayout layout = ValueControlLayout::NONE;

    bool apply(const SliderStyleProperties &src);
    SliderStyleProperties diff(const SliderStyleProperties &base) const;
};

struct TreeViewStyleProperties {
    float rowHeight = PROP_UNSET_FLOAT;
    UDim4 cellPadding = {{PROP_UNSET_FLOAT, 0}, {}, {}, {}};
    am_bool showColumnSeparators = PROP_UNSET_BOOL;
    float columnSeparatorWidth = PROP_UNSET_FLOAT;
    Color4 columnSeparatorColor = Color4(PROP_UNSET_FLOAT);
    am_bool showDisclosureTriangles = PROP_UNSET_BOOL;
    float disclosureTriangleSize = PROP_UNSET_FLOAT;
    float disclosureTrianglePadding = PROP_UNSET_FLOAT;
    Color4 disclosureTriangleColor = Color4(PROP_UNSET_FLOAT);
    float indentPerLevel = PROP_UNSET_FLOAT;
    Color4 rowBackgroundColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowAlternateColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowHoverColor = Color4(PROP_UNSET_FLOAT);
    Color4 rowSelectedColor = Color4(PROP_UNSET_FLOAT);
    am_bool fillRows = PROP_UNSET_BOOL;
    am_bool showHeader = PROP_UNSET_BOOL;
    float headerHeight = PROP_UNSET_FLOAT;
    Color3 headerColor = Color3(PROP_UNSET_FLOAT);
    TextStyleProperties header{};

    bool apply(const TreeViewStyleProperties &src);
    TreeViewStyleProperties diff(const TreeViewStyleProperties &base) const;
};

// Scope-input DTOs: bundle the layout/style/content/config a builder needs into one struct
// per component, so scope methods take a single brace-initialized argument.

struct CanvasProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
};

struct FrameProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
};

struct ScrollingFrameProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    ScrollingFrameStyleProperties scroll{};
};

struct TextLabelProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    TextStyleProperties text{};
    std::string label{};
};

struct TextButtonProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    TextStyleProperties text{};
    std::string label{};
    ButtonProperties button{};
};

struct ImageLabelProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    ImageStyleProperties image{};
    AmTextureId texture{};
    std::string svg{};
};

struct ImageButtonProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    ImageStyleProperties image{};
    AmTextureId texture{};
    std::string svg{};
    ButtonProperties button{};
};

struct InvisibleButtonProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
};

struct CheckboxProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    CheckboxStyleProperties checkbox{};
};

struct CollapsibleHeaderProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    CollapsibleHeaderStyleProperties header{};
    std::string title{};
};

struct TabBarProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    TabBarStyleProperties tabBar{};
};

struct TableProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    TableStyleProperties table{};
};

struct TextInputProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    TextInputStyleProperties textInput{};
    std::string placeholder{};
};

struct SliderProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    SliderStyleProperties slider{};
    std::string label{};
    std::string valueSuffix{};
};

struct TreeViewProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    TreeViewStyleProperties treeView{};
};

struct DropdownProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    TextStyleProperties text{};
    std::string label{};
    DropdownStyleProperties dropdown{};
};

struct MenuBarProperties {
    std::vector<std::string> classes{};
    BaseProperties base{};
    BaseStyleProperties style{};
    MenuBarStyleProperties menuBar{};
};

} // namespace Amethyst

#endif // AMETHYST__PROPERTIES_H
