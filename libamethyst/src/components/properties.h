#ifndef AMETHYST__PROPERTIES_H
#define AMETHYST__PROPERTIES_H

#include "components/common.h"

#include "math/math.h"
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Amethyst {

enum class FieldShape {
    Plain,
    Args,
    Mask
};

// optional<T> can't be list-initialized from a multi-value brace list like {1.0f, 1.0f, 1.0f}
// (its converting constructor only deduces a single argument), which breaks every Args field
// whose type is itself constructed from several values (colors, UDim2, corner radii, ...).
// AmArg forwards the whole argument pack into T's own constructor instead, so designated-init
// call sites don't need to change, while still exposing has_value()/value() like optional does.
template <typename T> struct AmArg {
    AmArg() = default;
    AmArg(const AmArg &) = default;
    AmArg(AmArg &&) = default;
    AmArg &operator=(const AmArg &) = default;
    AmArg &operator=(AmArg &&) = default;

    // Non-template overloads for the "already a T" case, so a source type with its own generic
    // conversion operator (e.g. vec2's operator T() for any T constructible from two floats)
    // doesn't tie against the variadic constructor below and make the conversion ambiguous.
    AmArg(const T &value) : m_value(value) {}
    AmArg(T &&value) : m_value(std::move(value)) {}

    template <typename... Args> AmArg(Args &&...args) : m_value(T{std::forward<Args>(args)...}) {}

    bool has_value() const { return m_value.has_value(); }
    const T &value() const { return m_value.value(); }

  private:
    std::optional<T> m_value{};
};

template <typename T> struct ArgsOfImpl {
    using type = AmArg<T>;
};
template <typename T> using ArgsOf = typename ArgsOfImpl<T>::type;

template <FieldShape Shape, typename T>
using AmField = std::conditional_t<Shape == FieldShape::Args, ArgsOf<T>, std::conditional_t<Shape == FieldShape::Mask, bool, T>>;

template <FieldShape Shape> struct BasePropertiesFields {
    template <typename T> using F = AmField<Shape, T>;

    F<bool> active{};
    F<vec2> anchorPoint{};
    F<AutomaticSize> automaticSize{};
    F<bool> clipsDescendants{};
    F<bool> interactable{};
    F<LayoutOrder> layoutOrder{};
    F<UDim4> padding{};
    F<UDim4> margin{};
    F<UDim2> position{};
    F<UDim2> size{};
    F<Degrees> rotation{};
    F<bool> visible{};
    F<int32_t> zIndex{};
    F<ZIndexBehavior> zindexBehavior{};
};

using BasePropertiesArgs = BasePropertiesFields<FieldShape::Args>;

struct BaseProperties : BasePropertiesFields<FieldShape::Plain> {
    bool apply(const BasePropertiesArgs &src);
    operator BasePropertiesArgs() const;
};

template <FieldShape Shape> struct ButtonPropertiesFields {
    template <typename T> using F = AmField<Shape, T>;

    F<bool> autoButtonColor{};
    F<bool> modal{};
};

using ButtonPropertiesArgs = ButtonPropertiesFields<FieldShape::Args>;

struct ButtonProperties : ButtonPropertiesFields<FieldShape::Plain> {
    bool apply(const ButtonPropertiesArgs &src);
};

template <FieldShape Shape> struct BaseStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<Color3> backgroundColor{};
    F<float> backgroundTransparency{};
    F<BorderMode> borderMode{};
    F<float> borderPixelSize{};
    F<Color3> borderColor{};
    F<float> borderTransparency{};
    F<uvec4> cornerRadius{};
};

using BaseStylePropertiesArgs = BaseStyleFields<FieldShape::Args>;
using BaseStyleOverrideMask = BaseStyleFields<FieldShape::Mask>;

struct BaseStyleProperties : BaseStyleFields<FieldShape::Plain> {
    BaseStyleOverrideMask overridden;

    bool apply(const BaseStyleProperties &src);
    bool apply(const BaseStylePropertiesArgs &src);

    operator BaseStylePropertiesArgs() const;
};

template <> struct ArgsOfImpl<BaseStyleProperties> {
    using type = BaseStylePropertiesArgs;
};

template <FieldShape Shape> struct TextStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<float> fontSize{};
    F<Color4> textColor{};
    F<TextXAlignment> textXAlignment{};
    F<TextYAlignment> textYAlignment{};
    F<TextTruncate> textTruncate{};
    F<bool> richText{};
    F<bool> textWrapped{};
    F<bool> textScaled{};
    F<float> lineHeight{};
    F<float> strokeThickness{};
    F<Color4> strokeColor{};
    F<std::string> fontFamily{};
};

using TextStylePropertiesArgs = TextStyleFields<FieldShape::Args>;
using TextStyleOverrideMask = TextStyleFields<FieldShape::Mask>;

struct TextStyleProperties : TextStyleFields<FieldShape::Plain> {
    TextStyleOverrideMask overridden;

    bool apply(const TextStyleProperties &src);
    bool apply(const TextStylePropertiesArgs &src);

    operator TextStylePropertiesArgs() const;
};

template <> struct ArgsOfImpl<TextStyleProperties> {
    using type = TextStylePropertiesArgs;
};

template <FieldShape Shape> struct ImageStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<Color4> imageColor{};
    F<ImageScaleType> scaleType{};
    F<vec2> tileSize{};
};

using ImageStylePropertiesArgs = ImageStyleFields<FieldShape::Args>;
using ImageStyleOverrideMask = ImageStyleFields<FieldShape::Mask>;

struct ImageStyleProperties : ImageStyleFields<FieldShape::Plain> {
    ImageStyleOverrideMask overridden;

    bool apply(const ImageStyleProperties &src);
    bool apply(const ImageStylePropertiesArgs &src);

    operator ImageStylePropertiesArgs() const;
};

template <FieldShape Shape> struct ScrollingFrameStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<ScrollAxis> scrollAxis{};
    F<ScrollBarVisibility> scrollBarVisibility{};
    F<UDim2> canvasSize{};
    F<UDim2> canvasPosition{};
    F<AutomaticSize> automaticCanvasSize{};
    F<Color3> scrollBarColor{};
    F<float> scrollBarTransparency{};
    F<float> scrollBarThickness{};
    F<Color3> scrollBarThumbColor{};
    F<float> scrollBarThumbTransparency{};
    F<float> scrollSpeed{};
    F<bool> elasticScrolling{};
};

using ScrollingFrameStylePropertiesArgs = ScrollingFrameStyleFields<FieldShape::Args>;
using ScrollingFrameStyleOverrideMask = ScrollingFrameStyleFields<FieldShape::Mask>;

struct ScrollingFrameStyleProperties : ScrollingFrameStyleFields<FieldShape::Plain> {
    ScrollingFrameStyleOverrideMask overridden;

    bool apply(const ScrollingFrameStyleProperties &src);
    bool apply(const ScrollingFrameStylePropertiesArgs &src);

    operator ScrollingFrameStylePropertiesArgs() const;
};

template <FieldShape Shape> struct CheckboxStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<Color4> checkColor{};
};

using CheckboxStylePropertiesArgs = CheckboxStyleFields<FieldShape::Args>;
using CheckboxStyleOverrideMask = CheckboxStyleFields<FieldShape::Mask>;

struct CheckboxStyleProperties : CheckboxStyleFields<FieldShape::Plain> {
    CheckboxStyleOverrideMask overridden;

    bool apply(const CheckboxStyleProperties &src);
    bool apply(const CheckboxStylePropertiesArgs &src);

    operator CheckboxStylePropertiesArgs() const;
};

template <FieldShape Shape> struct SplineStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<CurveType> type{};
    F<float> thickness{};
    F<Color4> color{};
    F<bool> showKnots{};
    F<float> knotSize{};
};

using SplineStylePropertiesArgs = SplineStyleFields<FieldShape::Args>;
using SplineStyleOverrideMask = SplineStyleFields<FieldShape::Mask>;

struct SplineStyleProperties : SplineStyleFields<FieldShape::Plain> {
    SplineStyleOverrideMask overridden;

    bool apply(const SplineStyleProperties &src);
    bool apply(const SplineStylePropertiesArgs &src);

    operator SplineStylePropertiesArgs() const;
};

template <FieldShape Shape> struct CollapsibleHeaderStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<bool> expanded{};
    F<TextStyleProperties> titleStyle{};
    F<float> headerHeight{};
    F<Color3> headerColor{};
    F<float> headerTransparency{};
    F<float> headerCornerRadius{};
    F<bool> showIndicator{};
    F<float> indicatorSize{};
    F<float> indicatorPadding{};
    F<Color4> indicatorColor{};
};

using CollapsibleHeaderStylePropertiesArgs = CollapsibleHeaderStyleFields<FieldShape::Args>;
using CollapsibleHeaderStyleOverrideMask = CollapsibleHeaderStyleFields<FieldShape::Mask>;

struct CollapsibleHeaderStyleProperties : CollapsibleHeaderStyleFields<FieldShape::Plain> {
    CollapsibleHeaderStyleOverrideMask overridden;

    bool apply(const CollapsibleHeaderStyleProperties &src);
    bool apply(const CollapsibleHeaderStylePropertiesArgs &src);

    operator CollapsibleHeaderStylePropertiesArgs() const;
};

template <FieldShape Shape> struct ContextMenuStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<Color3> itemHoverBackground{};
    F<Color3> separatorColor{};
};

using ContextMenuStylePropertiesArgs = ContextMenuStyleFields<FieldShape::Args>;
using ContextMenuStyleOverrideMask = ContextMenuStyleFields<FieldShape::Mask>;

struct ContextMenuStyleProperties : ContextMenuStyleFields<FieldShape::Plain> {
    ContextMenuStyleOverrideMask overridden;

    bool apply(const ContextMenuStyleProperties &src);
    bool apply(const ContextMenuStylePropertiesArgs &src);

    operator ContextMenuStylePropertiesArgs() const;
};

template <FieldShape Shape> struct DropdownStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<DropdownDirection> popupDirection{};
    F<int32_t> maxVisibleItems{};
    F<float> itemHeight{};
    F<float> popupWidth{};
    F<float> itemFontSize{};
    F<Color3> popupBackground{};
    F<Color4> itemTextColor{};
    F<Color4> itemDisabledColor{};
    F<Color3> itemHoverBackground{};
    F<Color3> separatorColor{};
};

using DropdownStylePropertiesArgs = DropdownStyleFields<FieldShape::Args>;
using DropdownStyleOverrideMask = DropdownStyleFields<FieldShape::Mask>;

struct DropdownStyleProperties : DropdownStyleFields<FieldShape::Plain> {
    DropdownStyleOverrideMask overridden;

    bool apply(const DropdownStyleProperties &src);
    bool apply(const DropdownStylePropertiesArgs &src);

    operator DropdownStylePropertiesArgs() const;
};

template <FieldShape Shape> struct TabBarStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<bool> closeable{};
    F<bool> persistLayout{};
    F<TabBarMode> mode{};
    F<TabBarPosition> tabPosition{};
    F<TabBarVisibility> visibility{};
    F<float> barThickness{};
    F<float> tabWidth{};
    F<float> tabSpacing{};
    F<float> tabOffset{};
    F<uvec4> tabCornerRadius{};
    F<Color4> closeColor{};
    F<TabCloseButtonVisibility> closeButtonVisibility{};
    F<bool> tabTearOffEnabled{};
};

using TabBarStylePropertiesArgs = TabBarStyleFields<FieldShape::Args>;
using TabBarStyleOverrideMask = TabBarStyleFields<FieldShape::Mask>;

struct TabBarStyleProperties : TabBarStyleFields<FieldShape::Plain> {
    TabBarStyleOverrideMask overridden;

    bool apply(const TabBarStyleProperties &src);
    bool apply(const TabBarStylePropertiesArgs &src);

    operator TabBarStylePropertiesArgs() const;
};

template <FieldShape Shape> struct MenuBarStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<float> entryPaddingX{};
    F<float> entryPaddingY{};
    F<float> entryFontSize{};
};

using MenuBarStylePropertiesArgs = MenuBarStyleFields<FieldShape::Args>;
using MenuBarStyleOverrideMask = MenuBarStyleFields<FieldShape::Mask>;

struct MenuBarStyleProperties : MenuBarStyleFields<FieldShape::Plain> {
    MenuBarStyleOverrideMask overridden;

    bool apply(const MenuBarStyleProperties &src);
    bool apply(const MenuBarStylePropertiesArgs &src);

    operator MenuBarStylePropertiesArgs() const;
};

template <FieldShape Shape> struct TextInputStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<TextStyleProperties> text{};
    F<Color4> placeholderColor{};
    F<Color4> selectionColor{};
    F<Color4> cursorColor{};
    F<bool> multiline{};
    F<int32_t> maxLength{};
    F<bool> readOnly{};
    F<float> cursorBlinkRate{};
};

using TextInputStylePropertiesArgs = TextInputStyleFields<FieldShape::Args>;
using TextInputStyleOverrideMask = TextInputStyleFields<FieldShape::Mask>;

struct TextInputStyleProperties : TextInputStyleFields<FieldShape::Plain> {
    TextInputStyleOverrideMask overridden;

    bool apply(const TextInputStyleProperties &src);
    bool apply(const TextInputStylePropertiesArgs &src);

    operator TextInputStylePropertiesArgs() const;
};

template <FieldShape Shape> struct TableStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<float> rowHeight{};
    F<UDim4> cellPadding{};
    F<TableSeparatorMode> separatorMode{};
    F<float> separatorWidth{};
    F<Color4> separatorColor{};
    F<bool> showHeader{};
    F<float> headerHeight{};
    F<Color3> headerColor{};
    F<TextStyleProperties> header{};
    F<Color4> rowBackgroundColor{};
    F<Color4> rowAlternateColor{};
    F<Color4> selectedRowColor{};
};

using TableStylePropertiesArgs = TableStyleFields<FieldShape::Args>;
using TableStyleOverrideMask = TableStyleFields<FieldShape::Mask>;

struct TableStyleProperties : TableStyleFields<FieldShape::Plain> {
    TableStyleOverrideMask overridden;

    bool apply(const TableStyleProperties &src);
    bool apply(const TableStylePropertiesArgs &src);

    operator TableStylePropertiesArgs() const;
};

template <FieldShape Shape> struct SliderStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<BaseStyleProperties> thumb{};
    F<TextStyleProperties> text{};
    F<UDim> trackHeight{};
    F<float> thumbWidth{};
    F<float> thumbHeight{};
    F<Color4> fillColor{};
    F<float> labelPadding{};
};

using SliderStylePropertiesArgs = SliderStyleFields<FieldShape::Args>;
using SliderStyleOverrideMask = SliderStyleFields<FieldShape::Mask>;

struct SliderStyleProperties : SliderStyleFields<FieldShape::Plain> {
    SliderStyleOverrideMask overridden;

    bool apply(const SliderStyleProperties &src);
    bool apply(const SliderStylePropertiesArgs &src);

    operator SliderStylePropertiesArgs() const;
};

template <FieldShape Shape> struct DragStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<TextStyleProperties> text{};
};

using DragStylePropertiesArgs = DragStyleFields<FieldShape::Args>;
using DragStyleOverrideMask = DragStyleFields<FieldShape::Mask>;

struct DragStyleProperties : DragStyleFields<FieldShape::Plain> {
    DragStyleOverrideMask overridden;

    bool apply(const DragStyleProperties &src);
    bool apply(const DragStylePropertiesArgs &src);

    operator DragStylePropertiesArgs() const;
};

template <FieldShape Shape> struct TreeViewStyleFields {
    template <typename T> using F = AmField<Shape, T>;

    F<float> rowHeight{};
    F<UDim4> cellPadding{};
    F<bool> showColumnSeparators{};
    F<float> columnSeparatorWidth{};
    F<Color4> columnSeparatorColor{};
    F<bool> showDisclosureTriangles{};
    F<float> disclosureTriangleSize{};
    F<float> disclosureTrianglePadding{};
    F<Color4> disclosureTriangleColor{};
    F<float> indentPerLevel{};
    F<Color4> rowBackgroundColor{};
    F<Color4> rowAlternateColor{};
    F<Color4> rowHoverColor{};
    F<Color4> rowSelectedColor{};
    F<bool> fillRows{};
    F<bool> showHeader{};
    F<float> headerHeight{};
    F<Color3> headerColor{};
    F<TextStyleProperties> header{};
};

using TreeViewStylePropertiesArgs = TreeViewStyleFields<FieldShape::Args>;
using TreeViewStyleOverrideMask = TreeViewStyleFields<FieldShape::Mask>;

struct TreeViewStyleProperties : TreeViewStyleFields<FieldShape::Plain> {
    TreeViewStyleOverrideMask overridden;

    bool apply(const TreeViewStyleProperties &src);
    bool apply(const TreeViewStylePropertiesArgs &src);

    operator TreeViewStylePropertiesArgs() const;
};

// Scope-input DTOs: bundle the layout/style/content/config a builder needs into one struct
// per component, so scope methods take a single brace-initialized argument.

struct CanvasProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
};

struct SplineProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    SplineStylePropertiesArgs spline{};
    std::vector<vec2> knots{};
};

struct FrameProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
};

struct ScrollingFrameProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    ScrollingFrameStylePropertiesArgs scroll{};
};

struct TextLabelProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextStylePropertiesArgs text{};
    std::string label{};
};

struct TextButtonProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextStylePropertiesArgs text{};
    std::string label{};
    ButtonPropertiesArgs button{};
};

struct ImageLabelProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    ImageStylePropertiesArgs image{};
    AmTextureId texture{};
    std::string svg{};
};

struct ImageButtonProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    ImageStylePropertiesArgs image{};
    AmTextureId texture{};
    std::string svg{};
    ButtonPropertiesArgs button{};
};

struct InvisibleButtonProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
};

struct PopupProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    PopupPlacement placement = PopupPlacement::BELOW;
    vec2 offset = vec2(0.0f);
    bool matchAnchorWidth = false;
    bool closeOnClickOutside = true;
};

struct CheckboxProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    CheckboxStylePropertiesArgs checkbox{};
    bool *value = nullptr;
};

struct CollapsibleHeaderProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    CollapsibleHeaderStylePropertiesArgs header{};
    std::string title{};
};

struct TabBarProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TabBarStylePropertiesArgs tabBar{};
};

struct TableProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TableStylePropertiesArgs table{};
};

struct TextInputProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextInputStylePropertiesArgs textInput{};
    std::string placeholder{};
};

struct NumberInputProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextInputStylePropertiesArgs textInput{};
    std::string placeholder{};
    bool allowDecimal = true;
    bool allowNegative = true;
};

struct SliderFloatProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    SliderStylePropertiesArgs slider{};
    std::string format{};
    ValueScale scale = ValueScale::LINEAR;
    ShapeKind thumbShape = ShapeKind::RECT;
    float min = 0.0f;
    float max = 100.0f;
    float *value = nullptr;
};

struct SliderIntProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    SliderStylePropertiesArgs slider{};
    std::string format{};
    ValueScale scale = ValueScale::LINEAR;
    ShapeKind thumbShape = ShapeKind::RECT;
    int min = 0;
    int max = 100;
    int *value = nullptr;
};

struct DragFloatProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    DragStylePropertiesArgs drag{};
    std::string format{};
    ValueScale scale = ValueScale::LINEAR;
    double speed = 1.0;
    double min = std::numeric_limits<double>::lowest();
    double max = std::numeric_limits<double>::max();
    double *value = nullptr;
};

struct DragIntProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    DragStylePropertiesArgs drag{};
    std::string format{};
    ValueScale scale = ValueScale::LINEAR;
    int64_t speed = 1;
    int64_t min = std::numeric_limits<int64_t>::min();
    int64_t max = std::numeric_limits<int64_t>::max();
    int64_t *value = nullptr;
};

struct Color3PickerProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    ColorModel model = ColorModel::HSV;
    ColorPickerShape shape = ColorPickerShape::SQUARE;
    Color3 *value = nullptr;
};

struct Color4PickerProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    ColorModel model = ColorModel::HSV;
    ColorPickerShape shape = ColorPickerShape::SQUARE;
    Color4 *value = nullptr;
};

struct TreeViewProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    TreeViewStylePropertiesArgs treeView{};
};

struct ContextMenuProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextStylePropertiesArgs text{};
    ContextMenuStylePropertiesArgs contextMenu{};
    int32_t maxVisibleItems = 8;
    float itemHeight = 24.0f;
    float popupWidth = 180.0f;
};

struct DropdownProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    TextStylePropertiesArgs text{};
    std::string label{};
    DropdownStylePropertiesArgs dropdown{};
};

struct MenuBarProperties {
    std::vector<std::string> classes{};
    BasePropertiesArgs base{};
    BaseStylePropertiesArgs style{};
    MenuBarStylePropertiesArgs menuBar{};
};

} // namespace Amethyst

#endif // AMETHYST__PROPERTIES_H
