#ifndef AMETHYST__STYLE_H
#define AMETHYST__STYLE_H

#include "components/common.h"
#include "modules/color.h"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <variant>

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

using StyleValue = std::variant<Color3, Color4, float, std::string, BorderMode, TextXAlignment, TextYAlignment, UDim>;

using StyleKey = uint32_t;

/**
 * @brief CSS-like style system with hierarchical lookup.
 *
 * Components query style values by property and type. Lookup walks the
 * type hierarchy (e.g. TEXT_BUTTON -> UI_BUTTON -> UI_OBJECT) until a
 * value is found or falls back to a default.
 */
class Style {
  public:
    Style() = default;

    static Style &instance();
    static bool load(const std::filesystem::path &path);

    /**
     * @brief Get a style value, walking the type hierarchy.
     * @tparam T The expected value type (Color3, float, etc.)
     * @param property The style property to look up.
     * @param type The component type (lookup walks its hierarchy).
     */
    template <typename T> T get(StyleProperty property, ComponentType type) const
    {
        for (ComponentType t : getTypeHierarchy(type)) {
            auto key = makeKey(property, t);
            auto it = m_values.find(key);
            if (it != m_values.end()) {
                return std::get<T>(it->second);
            }
        }
        return std::get<T>(getDefault(property));
    }

    template <typename T> void set(StyleProperty property, ComponentType type, const T &value)
    {
        m_values[makeKey(property, type)] = value;
    }

    bool hasValue(StyleProperty property, ComponentType type) const { return m_values.contains(makeKey(property, type)); }

    void clear() { m_values.clear(); }

    static std::span<const ComponentType> getTypeHierarchy(ComponentType type);
    static StyleValue getDefault(StyleProperty property);
    static const std::unordered_map<std::string, StyleProperty> &getPropertyNames();
    static const std::unordered_map<std::string, ComponentType> &getComponentTypeNames();

  private:
    static StyleKey makeKey(StyleProperty property, ComponentType type)
    {
        return (static_cast<StyleKey>(type) << 16) | static_cast<StyleKey>(property);
    }

    std::unordered_map<StyleKey, StyleValue> m_values;
};

} // namespace Amethyst

#endif // AMETHYST__STYLE_H
