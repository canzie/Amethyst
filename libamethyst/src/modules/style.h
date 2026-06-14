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

    THUMB_COLOR,
    THUMB_TRANSPARENCY,
    THUMB_CORNER_RADIUS,
    THUMB_WIDTH,
    THUMB_HEIGHT,
    TRACK_HEIGHT,

    CHECK_COLOR,
    CHECK_TRANSPARENCY,
    CHECKBOX_SIZE,

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
    HEADER_CORNER_RADIUS,

    INDICATOR_SIZE,
    INDICATOR_PADDING,
    INDICATOR_COLOR,

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
    TEXT_INPUT,
};

/**
 * @brief Interned font-family handle; an index into the Style font-name table.
 */
struct FontHandle {
    uint32_t id = 0;
    bool operator==(const FontHandle &) const = default;
};

using StyleValue = std::variant<Color3, Color4, float, UDim, BorderMode, TextXAlignment, TextYAlignment, FontHandle>;

/**
 * @brief Class-name hash (FNV-1a 32-bit).
 */
using StyleKey = uint32_t;

inline constexpr size_t NUM_STYLE_PROPS = static_cast<size_t>(StyleProperty::COUNT);

/**
 * @brief Fully-populated resolved set: one value per property, indexed by StyleProperty.
 */
using DenseSet = std::array<StyleValue, NUM_STYLE_PROPS>;

/**
 * @brief Sparse authored set: only the properties a scope defines.
 */
using SparseSet = std::vector<std::pair<StyleProperty, StyleValue>>;

/**
 * @brief Theme store and resolver. Components query their style via the per-struct getters.
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
