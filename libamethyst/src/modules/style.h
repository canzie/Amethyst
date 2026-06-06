#ifndef AMETHYST__STYLE_H
#define AMETHYST__STYLE_H

#include "components/common.h"
#include "components/properties.h"
#include "modules/color.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <list>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Amethyst {

enum class StyleProperty {
    BACKGROUND_COLOR,
    BACKGROUND_TRANSPARENCY,
    BORDER_COLOR,
    BORDER_TRANSPARENCY,
    BORDER_PIXEL_SIZE,
    BORDER_MODE,
    CORNER_RADIUS,

    PADDING_TOP,
    PADDING_RIGHT,
    PADDING_BOTTOM,
    PADDING_LEFT,

    FONT_FAMILY,
    FONT_SIZE,
    TEXT_COLOR,
    TEXT_X_ALIGNMENT,
    TEXT_Y_ALIGNMENT,
    LINE_HEIGHT,
    STROKE_THICKNESS,
    STROKE_COLOR,

    SCROLLBAR_COLOR,
    SCROLLBAR_TRANSPARENCY,
    SCROLLBAR_THICKNESS,
    SCROLLBAR_THUMB_COLOR,
    SCROLLBAR_THUMB_TRANSPARENCY,

    ROW_HEIGHT,
    CELL_PADDING_TOP,
    CELL_PADDING_RIGHT,
    CELL_PADDING_BOTTOM,
    CELL_PADDING_LEFT,
    COLUMN_SEPARATOR_WIDTH,
    COLUMN_SEPARATOR_COLOR,

    ROW_BACKGROUND_COLOR,
    ROW_ALTERNATE_COLOR,
    ROW_HOVER_COLOR,
    ROW_SELECTED_COLOR,
    INDENT_PER_LEVEL,
    DISCLOSURE_TRIANGLE_SIZE,
    DISCLOSURE_TRIANGLE_PADDING,
    DISCLOSURE_TRIANGLE_COLOR,

    SLIDER_COLOR,
    SLIDER_TRANSPARENCY,
    THUMB_COLOR,
    THUMB_TRANSPARENCY,
    TRACK_CORNER_RADIUS,
    THUMB_CORNER_RADIUS,

    CHECK_COLOR,
    CHECK_TRANSPARENCY,
    CHECKBOX_SIZE,

    LABEL_COLOR,
    LABEL_PADDING,
    VALUE_COLOR,

    HIGHLIGHT_COLOR,
    HIGHLIGHT_TRANSPARENCY,

    TAB_WIDTH,
    TAB_SPACING,
    BAR_THICKNESS,
    TAB_COLOR,
    TAB_ACTIVE_COLOR,
    TAB_HOVERED_COLOR,
    TAB_PRESSED_COLOR,

    HEADER_COLOR,
    HEADER_TRANSPARENCY,
    HEADER_HEIGHT,

    COUNT,
};

enum class ComponentType {
    UI_OBJECT,
    UI_BUTTON,
    UI_LABEL,

    FRAME,
    SCROLLING_FRAME,
    TABLE,
    TREE_VIEW,
    TEXT_BUTTON,
    IMAGE_BUTTON,
    TEXT_LABEL,
    IMAGE_LABEL,
    CANVAS,
    CHECKBOX,
    DROPDOWN,
    TAB_BAR,
    SLIDER,
    RADIO_BUTTON,
    COLLAPSIBLE_HEADER,
};

/**
 * @brief Interned font-family handle. Index into the Style font-name table.
 * Kept POD so StyleValue stays trivially copyable.
 */
struct FontHandle {
    uint32_t id = 0;
    bool operator==(const FontHandle &) const = default;
};

using StyleValue = std::variant<Color3, Color4, float, UDim, BorderMode, TextXAlignment, TextYAlignment, FontHandle>;

/// Class-name hash (FNV-1a 32). NOT the old packed (type<<16)|prop key.
using StyleKey = uint32_t;

inline constexpr size_t NUM_STYLE_PROPS = static_cast<size_t>(StyleProperty::COUNT);

/// Fully-populated resolved set: one value per property, indexed by StyleProperty.
using DenseSet = std::array<StyleValue, NUM_STYLE_PROPS>;

/// Sparse authored set: only the properties a scope defines.
using SparseSet = std::vector<std::pair<StyleProperty, StyleValue>>;

// ---------------------------------------------------------------------------
// Single source of truth: the global property table and per-struct field lists.
// Each row of AM_STYLE_PROPS is (ENUM, themeKey, CppType, defaultExpr). themeKey
// is an identifier stringified by the parser. Per-struct lists are (ENUM, field,
// CppType) and drive the getters. A property may appear in several struct lists.
// ---------------------------------------------------------------------------

#define AM_STYLE_PROPS(X)                                                                         \
    X(BACKGROUND_COLOR, backgroundColor, Color3, Color3(1.0f, 1.0f, 1.0f))                        \
    X(BACKGROUND_TRANSPARENCY, backgroundTransparency, float, 0.0f)                               \
    X(BORDER_COLOR, borderColor, Color3, Color3(0.0f, 0.0f, 0.0f))                                \
    X(BORDER_TRANSPARENCY, borderTransparency, float, 0.0f)                                       \
    X(BORDER_PIXEL_SIZE, borderPixelSize, float, 0.0f)                                            \
    X(BORDER_MODE, borderMode, BorderMode, BorderMode::OUTLINE)                                   \
    X(CORNER_RADIUS, cornerRadius, float, 0.0f)                                                   \
    X(PADDING_TOP, paddingTop, UDim, UDim{})                                                      \
    X(PADDING_RIGHT, paddingRight, UDim, UDim{})                                                  \
    X(PADDING_BOTTOM, paddingBottom, UDim, UDim{})                                                \
    X(PADDING_LEFT, paddingLeft, UDim, UDim{})                                                    \
    X(FONT_FAMILY, fontFamily, FontHandle, FontHandle{0})                                         \
    X(FONT_SIZE, fontSize, float, 14.0f)                                                          \
    X(TEXT_COLOR, textColor, Color4, Color4(0.0f, 0.0f, 0.0f, 1.0f))                              \
    X(TEXT_X_ALIGNMENT, textXAlignment, TextXAlignment, TextXAlignment::LEFT)                     \
    X(TEXT_Y_ALIGNMENT, textYAlignment, TextYAlignment, TextYAlignment::TOP)                      \
    X(LINE_HEIGHT, lineHeight, float, 1.0f)                                                       \
    X(STROKE_THICKNESS, strokeThickness, float, 0.0f)                                             \
    X(STROKE_COLOR, strokeColor, Color4, Color4(0.0f, 0.0f, 0.0f, 1.0f))                          \
    X(SCROLLBAR_COLOR, scrollbarColor, Color3, Color3(0.7f))                                      \
    X(SCROLLBAR_TRANSPARENCY, scrollbarTransparency, float, 0.0f)                                 \
    X(SCROLLBAR_THICKNESS, scrollbarThickness, float, 8.0f)                                       \
    X(SCROLLBAR_THUMB_COLOR, scrollbarThumbColor, Color3, Color3(0.5f))                           \
    X(SCROLLBAR_THUMB_TRANSPARENCY, scrollbarThumbTransparency, float, 0.0f)                      \
    X(ROW_HEIGHT, rowHeight, float, 0.0f)                                                         \
    X(CELL_PADDING_TOP, cellPaddingTop, UDim, UDim{})                                             \
    X(CELL_PADDING_RIGHT, cellPaddingRight, UDim, UDim{})                                         \
    X(CELL_PADDING_BOTTOM, cellPaddingBottom, UDim, UDim{})                                       \
    X(CELL_PADDING_LEFT, cellPaddingLeft, UDim, UDim{})                                           \
    X(COLUMN_SEPARATOR_WIDTH, columnSeparatorWidth, float, 1.0f)                                  \
    X(COLUMN_SEPARATOR_COLOR, columnSeparatorColor, Color4, Color4(0.3f, 0.3f, 0.3f, 1.0f))       \
    X(ROW_BACKGROUND_COLOR, rowBackgroundColor, Color4, Color4(0.18f, 0.18f, 0.2f, 1.0f))         \
    X(ROW_ALTERNATE_COLOR, rowAlternateColor, Color4, Color4(0.22f, 0.22f, 0.24f, 1.0f))          \
    X(ROW_HOVER_COLOR, rowHoverColor, Color4, Color4(0.3f, 0.3f, 0.35f, 1.0f))                    \
    X(ROW_SELECTED_COLOR, rowSelectedColor, Color4, Color4(0.25f, 0.4f, 0.65f, 1.0f))             \
    X(INDENT_PER_LEVEL, indentPerLevel, float, 16.0f)                                             \
    X(DISCLOSURE_TRIANGLE_SIZE, disclosureTriangleSize, float, 10.0f)                             \
    X(DISCLOSURE_TRIANGLE_PADDING, disclosureTrianglePadding, float, 4.0f)                        \
    X(DISCLOSURE_TRIANGLE_COLOR, disclosureTriangleColor, Color4, Color4(0.7f, 0.7f, 0.7f, 1.0f)) \
    X(SLIDER_COLOR, sliderColor, Color3, Color3(0.5f))                                            \
    X(SLIDER_TRANSPARENCY, sliderTransparency, float, 0.0f)                                       \
    X(THUMB_COLOR, thumbColor, Color3, Color3(0.8f))                                              \
    X(THUMB_TRANSPARENCY, thumbTransparency, float, 0.0f)                                         \
    X(TRACK_CORNER_RADIUS, trackCornerRadius, float, 0.0f)                                        \
    X(THUMB_CORNER_RADIUS, thumbCornerRadius, float, 0.0f)                                        \
    X(CHECK_COLOR, checkColor, Color3, Color3(0.0f, 0.0f, 0.0f))                                  \
    X(CHECK_TRANSPARENCY, checkTransparency, float, 0.0f)                                         \
    X(CHECKBOX_SIZE, checkboxSize, float, 20.0f)                                                  \
    X(LABEL_COLOR, labelColor, Color4, Color4(0.0f, 0.0f, 0.0f, 1.0f))                            \
    X(LABEL_PADDING, labelPadding, UDim, UDim::fromOffset(5.0f))                                  \
    X(VALUE_COLOR, valueColor, Color4, Color4(0.0f, 0.0f, 0.0f, 1.0f))                            \
    X(HIGHLIGHT_COLOR, highlightColor, Color3, Color3(0.7f, 0.7f, 0.9f))                          \
    X(HIGHLIGHT_TRANSPARENCY, highlightTransparency, float, 0.0f)                                 \
    X(TAB_WIDTH, tabWidth, float, 100.0f)                                                         \
    X(TAB_SPACING, tabSpacing, float, 0.0f)                                                       \
    X(BAR_THICKNESS, barThickness, float, 30.0f)                                                  \
    X(TAB_COLOR, tabColor, Color3, Color3(0.22f))                                                 \
    X(TAB_ACTIVE_COLOR, tabActiveColor, Color3, Color3(0.32f))                                    \
    X(TAB_HOVERED_COLOR, tabHoveredColor, Color3, Color3(0.28f))                                  \
    X(TAB_PRESSED_COLOR, tabPressedColor, Color3, Color3(0.18f))                                  \
    X(HEADER_COLOR, headerColor, Color3, Color3(0.25f, 0.25f, 0.28f))                             \
    X(HEADER_TRANSPARENCY, headerTransparency, float, 0.0f)                                       \
    X(HEADER_HEIGHT, headerHeight, float, 30.0f)

#define AM_BASE_STYLE_FIELDS(X)                               \
    X(BACKGROUND_COLOR, backgroundColor, Color3)              \
    X(BACKGROUND_TRANSPARENCY, backgroundTransparency, float) \
    X(BORDER_MODE, borderMode, BorderMode)                    \
    X(BORDER_PIXEL_SIZE, borderPixelSize, float)              \
    X(BORDER_COLOR, borderColor, Color3)                      \
    X(BORDER_TRANSPARENCY, borderTransparency, float)         \
    X(CORNER_RADIUS, cornerRadius, float)

// fontFamily (optional<string> via FontHandle) is special-cased in the getter, not here.
#define AM_TEXT_STYLE_FIELDS(X)                         \
    X(FONT_SIZE, fontSize, float)                       \
    X(TEXT_COLOR, textColor, Color4)                    \
    X(TEXT_X_ALIGNMENT, textXAlignment, TextXAlignment) \
    X(TEXT_Y_ALIGNMENT, textYAlignment, TextYAlignment) \
    X(LINE_HEIGHT, lineHeight, float)                   \
    X(STROKE_THICKNESS, strokeThickness, float)         \
    X(STROKE_COLOR, strokeColor, Color4)

#define AM_SCROLLING_FRAME_STYLE_FIELDS(X)                  \
    X(SCROLLBAR_COLOR, scrollBarColor, Color3)              \
    X(SCROLLBAR_TRANSPARENCY, scrollBarTransparency, float) \
    X(SCROLLBAR_THICKNESS, scrollBarThickness, float)       \
    X(SCROLLBAR_THUMB_COLOR, scrollBarThumbColor, Color3)   \
    X(SCROLLBAR_THUMB_TRANSPARENCY, scrollBarThumbTransparency, float)

#define AM_SLIDER_STYLE_FIELDS(X)                     \
    X(SLIDER_COLOR, sliderColor, Color3)              \
    X(SLIDER_TRANSPARENCY, sliderTransparency, float) \
    X(THUMB_COLOR, thumbColor, Color3)                \
    X(THUMB_TRANSPARENCY, thumbTransparency, float)   \
    X(TRACK_CORNER_RADIUS, trackCornerRadius, float)  \
    X(THUMB_CORNER_RADIUS, thumbCornerRadius, float)  \
    X(LABEL_COLOR, labelColor, Color4)                \
    X(LABEL_PADDING, labelPadding, UDim)              \
    X(VALUE_COLOR, valueColor, Color4)                \
    X(FONT_SIZE, fontSize, float)

#define AM_TAB_BAR_STYLE_FIELDS(X)                \
    X(BAR_THICKNESS, barThickness, float)         \
    X(TAB_WIDTH, tabWidth, float)                 \
    X(TAB_SPACING, tabSpacing, float)             \
    X(TAB_COLOR, tabColor, Color3)                \
    X(TAB_ACTIVE_COLOR, focussedTabColor, Color3) \
    X(TAB_HOVERED_COLOR, hoveredTabColor, Color3) \
    X(TAB_PRESSED_COLOR, pressedTabColor, Color3)

#define AM_TABLE_STYLE_FIELDS(X)                            \
    X(ROW_HEIGHT, rowHeight, float)                         \
    X(COLUMN_SEPARATOR_WIDTH, columnSeparatorWidth, float)  \
    X(COLUMN_SEPARATOR_COLOR, columnSeparatorColor, Color4) \
    X(HEADER_HEIGHT, headerHeight, float)                   \
    X(HEADER_COLOR, headerColor, Color3)                    \
    X(ROW_BACKGROUND_COLOR, rowBackgroundColor, Color4)     \
    X(ROW_ALTERNATE_COLOR, rowAlternateColor, Color4)       \
    X(ROW_HOVER_COLOR, rowHoverColor, Color4)               \
    X(ROW_SELECTED_COLOR, rowSelectedColor, Color4)

#define AM_TREE_VIEW_STYLE_FIELDS(X)                                 \
    X(ROW_HEIGHT, rowHeight, float)                                  \
    X(DISCLOSURE_TRIANGLE_SIZE, disclosureTriangleSize, float)       \
    X(DISCLOSURE_TRIANGLE_PADDING, disclosureTrianglePadding, float) \
    X(DISCLOSURE_TRIANGLE_COLOR, disclosureTriangleColor, Color4)    \
    X(INDENT_PER_LEVEL, indentPerLevel, float)                       \
    X(ROW_BACKGROUND_COLOR, rowBackgroundColor, Color4)              \
    X(ROW_ALTERNATE_COLOR, rowAlternateColor, Color4)                \
    X(ROW_HOVER_COLOR, rowHoverColor, Color4)                        \
    X(ROW_SELECTED_COLOR, rowSelectedColor, Color4)

#define AM_CHECKBOX_STYLE_FIELDS(X)                 \
    X(CHECK_COLOR, checkColor, Color3)              \
    X(CHECK_TRANSPARENCY, checkTransparency, float) \
    X(CHECKBOX_SIZE, checkboxSize, float)

#define AM_COLLAPSIBLE_HEADER_STYLE_FIELDS(X) \
    X(HEADER_HEIGHT, headerHeight, float)     \
    X(HEADER_COLOR, headerColor, Color3)      \
    X(HEADER_TRANSPARENCY, headerTransparency, float)

/**
 * @brief Theme store + resolver.
 *
 * Storage is per-scope sparse sets of property->value. The type hierarchy is
 * baked once into dense full sets; classes (standalone + type-qualified) merge
 * on top at resolve time, cached by (type, class-set). Components consume via
 * the per-struct getters; never by property.
 */
class Style {
  public:
    Style();

    /**
     * @brief Access the global theme instance consulted by every component.
     * @return Reference to the singleton, replaced wholesale by load()
     */
    static Style &instance();

    /**
     * @brief Parse a theme file and replace the global instance on success.
     * @param path Filesystem path to the theme file
     * @return True if the file parsed and was installed, false on parse failure
     */
    static bool load(const std::filesystem::path &path);

    /**
     * @brief Resolve the base surface style for a component type and class set.
     *
     * Starts from the baked type cascade, then layers the matching standalone and
     * type-qualified class rules in precedence order; the result is a concrete value
     * for every themeable field. Instance overrides are applied by the caller on top.
     * Resolved sets are cached per (type, class-set).
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved base style
     */
    BaseStyleProperties getBaseStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the text style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved text style, with fontFamily decoded from its handle
     */
    TextStyleProperties getTextStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the scrolling-frame style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved scrolling-frame style
     */
    ScrollingFrameStyleProperties getScrollingFrameStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the slider style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved slider style
     */
    SliderStyleProperties getSliderStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the tab-bar style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved tab-bar style
     */
    TabBarStyleProperties getTabBarStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the table style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved table style
     */
    TableStyleProperties getTableStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the tree-view style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved tree-view style
     */
    TreeViewStyleProperties getTreeViewStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the checkbox style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved checkbox style
     */
    CheckboxStyleProperties getCheckboxStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Resolve the collapsible-header style for a component type and class set.
     * @param type Component type whose inheritance chain is walked
     * @param classes Class hashes carried by the node, in any order
     * @return Fully-resolved collapsible-header style
     */
    CollapsibleHeaderStyleProperties getCollapsibleHeaderStyle(ComponentType type, std::span<const StyleKey> classes = {});

    /**
     * @brief Hash a class name to its token. Stateless; safe without a loaded theme.
     * @param name Class name to hash
     * @return FNV-1a 32-bit hash of the name
     */
    static StyleKey classToken(std::string_view name);

    /**
     * @brief Record a class-name string for diagnostics. Asserts on a hash collision.
     * @param token The token produced by classToken(name)
     * @param name The class name the token was derived from
     */
    void registerClassName(StyleKey token, std::string_view name);

    /**
     * @brief Intern a font-family name to a stable handle.
     * @param name Font-family name; the empty/"default" name maps to id 0
     * @return Index into the font-name table
     */
    uint32_t internFont(std::string_view name);

    /**
     * @brief Resolve a font handle back to its family name.
     * @param handle Handle returned by internFont; out-of-range falls back to the default
     * @return The interned family name
     */
    const std::string &fontName(FontHandle handle) const;

    /**
     * @brief Add a property value to a component-type scope. Parser-facing.
     * @param type Component type the rule targets
     * @param prop Property being set
     * @param value Parsed value
     */
    void addTypeValue(ComponentType type, StyleProperty prop, const StyleValue &value);

    /**
     * @brief Add a property value to a standalone class scope. Parser-facing.
     * @param classToken Token of the class the rule targets
     * @param order Theme source order of the rule, used to break multi-class ties
     * @param prop Property being set
     * @param value Parsed value
     */
    void addClassValue(StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value);

    /**
     * @brief Add a property value to a type-qualified class scope (type.class). Parser-facing.
     * @param type Component type the rule is qualified by
     * @param classToken Token of the class the rule targets
     * @param order Theme source order of the rule, used to break ties
     * @param prop Property being set
     * @param value Parsed value
     */
    void addTypeClassValue(ComponentType type, StyleKey classToken, uint32_t order, StyleProperty prop, const StyleValue &value);

    /**
     * @brief Drop the baked type sets and merge cache after the theme is mutated.
     */
    void clearResolved();

    /**
     * @brief Get the type inheritance chain for a component.
     * @param type Component type to expand
     * @return Span of the chain ordered specific to general
     */
    static std::span<const ComponentType> getTypeHierarchy(ComponentType type);

    /**
     * @brief Get the theme section-name to component-type map.
     * @return Map from section header string to component type
     */
    static const std::unordered_map<std::string, ComponentType> &getComponentTypeNames();

  private:
    static uint64_t typeClassKey(ComponentType type, StyleKey classToken);

    const DenseSet &bakedFor(ComponentType type);
    const DenseSet &resolveSet(ComponentType type, std::span<const StyleKey> classes);
    void buildDefaults();

    struct CacheKey {
        ComponentType type;
        std::vector<StyleKey> classes; // sorted
        bool operator==(const CacheKey &) const = default;
    };
    struct CacheKeyHash {
        size_t operator()(const CacheKey &k) const;
    };

    DenseSet m_defaults;
    std::unordered_map<ComponentType, SparseSet> m_rawType;
    std::unordered_map<ComponentType, DenseSet> m_typeResolved; // lazily baked

    std::unordered_map<StyleKey, SparseSet> m_classSets;
    std::unordered_map<StyleKey, uint32_t> m_classOrder;
    std::unordered_map<uint64_t, SparseSet> m_typeClassSets; // key = typeClassKey(type, classHash)
    std::unordered_map<uint64_t, uint32_t> m_typeClassOrder;

    std::vector<std::string> m_fontNames;
    std::unordered_map<std::string, uint32_t> m_fontIndex;
    std::unordered_map<StyleKey, std::string> m_classNames;

    static constexpr size_t CACHE_CAP = 256;
    std::list<std::pair<CacheKey, DenseSet>> m_lru;
    std::unordered_map<CacheKey, std::list<std::pair<CacheKey, DenseSet>>::iterator, CacheKeyHash> m_cacheIndex;
};

} // namespace Amethyst

#endif // AMETHYST__STYLE_H
