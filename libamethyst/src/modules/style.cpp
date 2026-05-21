#include "modules/style.h"
#include "parsers/am_theme/am_theme_parser.h"

#include <array>

namespace Amethyst {

static Style s_instance;

Style &Style::instance()
{
    return s_instance;
}

bool Style::load(const std::filesystem::path &path)
{
    auto result = AmThemeParser::parseFile(path);
    if (result) {
        s_instance = std::move(*result);
        return true;
    }
    return false;
}

std::span<const ComponentType> Style::getTypeHierarchy(ComponentType type)
{
    static const std::array<ComponentType, 1> uiObject = {ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> uiButton = {ComponentType::UI_BUTTON, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> uiLabel = {ComponentType::UI_LABEL, ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 2> frame = {ComponentType::FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> scrollingFrame = {ComponentType::SCROLLING_FRAME, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> table = {ComponentType::TABLE, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> treeView = {ComponentType::TREE_VIEW, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> canvas = {ComponentType::CANVAS, ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> textButton = {ComponentType::TEXT_BUTTON, ComponentType::UI_BUTTON,
                                                            ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> imageButton = {ComponentType::IMAGE_BUTTON, ComponentType::UI_BUTTON,
                                                             ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> textLabel = {ComponentType::TEXT_LABEL, ComponentType::UI_LABEL,
                                                           ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> imageLabel = {ComponentType::IMAGE_LABEL, ComponentType::UI_LABEL,
                                                            ComponentType::UI_OBJECT};

    static const std::array<ComponentType, 3> checkbox = {ComponentType::CHECKBOX, ComponentType::UI_BUTTON,
                                                          ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> dropdown = {ComponentType::DROPDOWN, ComponentType::UI_BUTTON,
                                                          ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> tabBar = {ComponentType::TAB_BAR, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 2> slider = {ComponentType::SLIDER, ComponentType::UI_OBJECT};
    static const std::array<ComponentType, 3> radioButton = {ComponentType::RADIO_BUTTON, ComponentType::UI_BUTTON,
                                                             ComponentType::UI_OBJECT};

    switch (type) {
    case ComponentType::UI_OBJECT:
        return uiObject;
    case ComponentType::UI_BUTTON:
        return uiButton;
    case ComponentType::UI_LABEL:
        return uiLabel;
    case ComponentType::FRAME:
        return frame;
    case ComponentType::SCROLLING_FRAME:
        return scrollingFrame;
    case ComponentType::TABLE:
        return table;
    case ComponentType::TREE_VIEW:
        return treeView;
    case ComponentType::TEXT_BUTTON:
        return textButton;
    case ComponentType::IMAGE_BUTTON:
        return imageButton;
    case ComponentType::TEXT_LABEL:
        return textLabel;
    case ComponentType::IMAGE_LABEL:
        return imageLabel;
    case ComponentType::CANVAS:
        return canvas;
    case ComponentType::CHECKBOX:
        return checkbox;
    case ComponentType::DROPDOWN:
        return dropdown;
    case ComponentType::TAB_BAR:
        return tabBar;
    case ComponentType::SLIDER:
        return slider;
    case ComponentType::RADIO_BUTTON:
        return radioButton;
    }
    return uiObject;
}

StyleValue Style::getDefault(StyleProperty property)
{
    switch (property) {
    case StyleProperty::BACKGROUND_COLOR:
        return Color3(1.0f, 1.0f, 1.0f);
    case StyleProperty::BACKGROUND_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::BORDER_COLOR:
        return Color3(0.0f, 0.0f, 0.0f);
    case StyleProperty::BORDER_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::BORDER_PIXEL_SIZE:
        return 0.0f;
    case StyleProperty::BORDER_MODE:
        return BorderMode::OUTLINE;
    case StyleProperty::CORNER_RADIUS:
        return 0.0f;

    case StyleProperty::PADDING_TOP:
        return UDim{};
    case StyleProperty::PADDING_RIGHT:
        return UDim{};
    case StyleProperty::PADDING_BOTTOM:
        return UDim{};
    case StyleProperty::PADDING_LEFT:
        return UDim{};

    case StyleProperty::FONT_FAMILY:
        return std::string("default");
    case StyleProperty::FONT_SIZE:
        return 14.0f;
    case StyleProperty::TEXT_COLOR:
        return Color4(0.0f, 0.0f, 0.0f, 1.0f);
    case StyleProperty::TEXT_X_ALIGNMENT:
        return TextXAlignment::LEFT;
    case StyleProperty::TEXT_Y_ALIGNMENT:
        return TextYAlignment::TOP;
    case StyleProperty::LINE_HEIGHT:
        return 1.0f;
    case StyleProperty::STROKE_THICKNESS:
        return 0.0f;
    case StyleProperty::STROKE_COLOR:
        return Color4(0.0f, 0.0f, 0.0f, 1.0f);

    case StyleProperty::SCROLLBAR_COLOR:
        return Color3(0.7f, 0.7f, 0.7f);
    case StyleProperty::SCROLLBAR_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::SCROLLBAR_THICKNESS:
        return 8.0f;
    case StyleProperty::SCROLLBAR_THUMB_COLOR:
        return Color3(0.5f, 0.5f, 0.5f);
    case StyleProperty::SCROLLBAR_THUMB_TRANSPARENCY:
        return 0.0f;

    case StyleProperty::ROW_HEIGHT:
        return 0.0f;
    case StyleProperty::CELL_PADDING_TOP:
        return UDim{};
    case StyleProperty::CELL_PADDING_RIGHT:
        return UDim{};
    case StyleProperty::CELL_PADDING_BOTTOM:
        return UDim{};
    case StyleProperty::CELL_PADDING_LEFT:
        return UDim{};
    case StyleProperty::COLUMN_SEPARATOR_WIDTH:
        return 1.0f;
    case StyleProperty::COLUMN_SEPARATOR_COLOR:
        return Color4(0.3f, 0.3f, 0.3f, 1.0f);

    case StyleProperty::ROW_BACKGROUND_COLOR:
        return Color4(0.18f, 0.18f, 0.2f, 1.0f);
    case StyleProperty::ROW_ALTERNATE_COLOR:
        return Color4(0.22f, 0.22f, 0.24f, 1.0f);
    case StyleProperty::ROW_HOVER_COLOR:
        return Color4(0.3f, 0.3f, 0.35f, 1.0f);
    case StyleProperty::ROW_SELECTED_COLOR:
        return Color4(0.25f, 0.4f, 0.65f, 1.0f);
    case StyleProperty::INDENT_PER_LEVEL:
        return 16.0f;
    case StyleProperty::DISCLOSURE_TRIANGLE_SIZE:
        return 10.0f;
    case StyleProperty::DISCLOSURE_TRIANGLE_PADDING:
        return 4.0f;
    case StyleProperty::DISCLOSURE_TRIANGLE_COLOR:
        return Color4(0.7f, 0.7f, 0.7f, 1.0f);

    case StyleProperty::SLIDER_COLOR:
        return Color3(0.5f, 0.5f, 0.5f);
    case StyleProperty::SLIDER_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::THUMB_COLOR:
        return Color3(0.8f, 0.8f, 0.8f);
    case StyleProperty::THUMB_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::TRACK_CORNER_RADIUS:
        return 0.0f;
    case StyleProperty::THUMB_CORNER_RADIUS:
        return 0.0f;

    case StyleProperty::CHECK_COLOR:
        return Color3(0.0f, 0.0f, 0.0f);
    case StyleProperty::CHECK_TRANSPARENCY:
        return 0.0f;
    case StyleProperty::CHECKBOX_SIZE:
        return 20.0f;

    case StyleProperty::LABEL_COLOR:
        return Color4(0.0f, 0.0f, 0.0f, 1.0f);
    case StyleProperty::LABEL_PADDING:
        return UDim::fromOffset(5.0f);
    case StyleProperty::VALUE_COLOR:
        return Color4(0.0f, 0.0f, 0.0f, 1.0f);

    case StyleProperty::HIGHLIGHT_COLOR:
        return Color3(0.7f, 0.7f, 0.9f);
    case StyleProperty::HIGHLIGHT_TRANSPARENCY:
        return 0.0f;

    case StyleProperty::TAB_WIDTH:
        return 100.0f;
    case StyleProperty::BAR_THICKNESS:
        return 30.0f;
    case StyleProperty::TAB_COLOR:
        return Color3(0.22f, 0.22f, 0.22f);
    case StyleProperty::TAB_ACTIVE_COLOR:
        return Color3(0.32f, 0.32f, 0.32f);
    case StyleProperty::TAB_HOVERED_COLOR:
        return Color3(0.28f, 0.28f, 0.28f);
    case StyleProperty::TAB_PRESSED_COLOR:
        return Color3(0.18f, 0.18f, 0.18f);
    }
    return 0.0f;
}

const std::unordered_map<std::string, StyleProperty> &Style::getPropertyNames()
{
    static const std::unordered_map<std::string, StyleProperty> names = {
        {"backgroundColor", StyleProperty::BACKGROUND_COLOR},
        {"backgroundTransparency", StyleProperty::BACKGROUND_TRANSPARENCY},
        {"borderColor", StyleProperty::BORDER_COLOR},
        {"borderTransparency", StyleProperty::BORDER_TRANSPARENCY},
        {"borderPixelSize", StyleProperty::BORDER_PIXEL_SIZE},
        {"borderMode", StyleProperty::BORDER_MODE},
        {"cornerRadius", StyleProperty::CORNER_RADIUS},

        {"paddingTop", StyleProperty::PADDING_TOP},
        {"paddingRight", StyleProperty::PADDING_RIGHT},
        {"paddingBottom", StyleProperty::PADDING_BOTTOM},
        {"paddingLeft", StyleProperty::PADDING_LEFT},

        {"fontFamily", StyleProperty::FONT_FAMILY},
        {"fontSize", StyleProperty::FONT_SIZE},
        {"textColor", StyleProperty::TEXT_COLOR},
        {"textXAlignment", StyleProperty::TEXT_X_ALIGNMENT},
        {"textYAlignment", StyleProperty::TEXT_Y_ALIGNMENT},
        {"lineHeight", StyleProperty::LINE_HEIGHT},
        {"strokeThickness", StyleProperty::STROKE_THICKNESS},
        {"strokeColor", StyleProperty::STROKE_COLOR},

        {"scrollbarColor", StyleProperty::SCROLLBAR_COLOR},
        {"scrollbarTransparency", StyleProperty::SCROLLBAR_TRANSPARENCY},
        {"scrollbarThickness", StyleProperty::SCROLLBAR_THICKNESS},
        {"scrollbarThumbColor", StyleProperty::SCROLLBAR_THUMB_COLOR},
        {"scrollbarThumbTransparency", StyleProperty::SCROLLBAR_THUMB_TRANSPARENCY},

        {"rowHeight", StyleProperty::ROW_HEIGHT},
        {"cellPaddingTop", StyleProperty::CELL_PADDING_TOP},
        {"cellPaddingRight", StyleProperty::CELL_PADDING_RIGHT},
        {"cellPaddingBottom", StyleProperty::CELL_PADDING_BOTTOM},
        {"cellPaddingLeft", StyleProperty::CELL_PADDING_LEFT},
        {"columnSeparatorWidth", StyleProperty::COLUMN_SEPARATOR_WIDTH},
        {"columnSeparatorColor", StyleProperty::COLUMN_SEPARATOR_COLOR},

        {"rowBackgroundColor", StyleProperty::ROW_BACKGROUND_COLOR},
        {"rowAlternateColor", StyleProperty::ROW_ALTERNATE_COLOR},
        {"rowHoverColor", StyleProperty::ROW_HOVER_COLOR},
        {"rowSelectedColor", StyleProperty::ROW_SELECTED_COLOR},
        {"indentPerLevel", StyleProperty::INDENT_PER_LEVEL},
        {"disclosureTriangleSize", StyleProperty::DISCLOSURE_TRIANGLE_SIZE},
        {"disclosureTrianglePadding", StyleProperty::DISCLOSURE_TRIANGLE_PADDING},
        {"disclosureTriangleColor", StyleProperty::DISCLOSURE_TRIANGLE_COLOR},

        {"sliderColor", StyleProperty::SLIDER_COLOR},
        {"sliderTransparency", StyleProperty::SLIDER_TRANSPARENCY},
        {"thumbColor", StyleProperty::THUMB_COLOR},
        {"thumbTransparency", StyleProperty::THUMB_TRANSPARENCY},
        {"trackCornerRadius", StyleProperty::TRACK_CORNER_RADIUS},
        {"thumbCornerRadius", StyleProperty::THUMB_CORNER_RADIUS},

        {"checkColor", StyleProperty::CHECK_COLOR},
        {"checkTransparency", StyleProperty::CHECK_TRANSPARENCY},
        {"checkboxSize", StyleProperty::CHECKBOX_SIZE},

        {"labelColor", StyleProperty::LABEL_COLOR},
        {"labelPadding", StyleProperty::LABEL_PADDING},
        {"valueColor", StyleProperty::VALUE_COLOR},

        {"highlightColor", StyleProperty::HIGHLIGHT_COLOR},
        {"highlightTransparency", StyleProperty::HIGHLIGHT_TRANSPARENCY},

        {"tabWidth", StyleProperty::TAB_WIDTH},
        {"barThickness", StyleProperty::BAR_THICKNESS},
        {"tabColor", StyleProperty::TAB_COLOR},
        {"tabActiveColor", StyleProperty::TAB_ACTIVE_COLOR},
        {"tabHoveredColor", StyleProperty::TAB_HOVERED_COLOR},
        {"tabPressedColor", StyleProperty::TAB_PRESSED_COLOR},
    };
    return names;
}

const std::unordered_map<std::string, ComponentType> &Style::getComponentTypeNames()
{
    static const std::unordered_map<std::string, ComponentType> names = {
        {"general", ComponentType::UI_OBJECT},
        {"buttons", ComponentType::UI_BUTTON},
        {"labels", ComponentType::UI_LABEL},
        {"frames", ComponentType::FRAME},
        {"scrollingFrames", ComponentType::SCROLLING_FRAME},
        {"tables", ComponentType::TABLE},
        {"treeViews", ComponentType::TREE_VIEW},
        {"textButtons", ComponentType::TEXT_BUTTON},
        {"imageButtons", ComponentType::IMAGE_BUTTON},
        {"textLabels", ComponentType::TEXT_LABEL},
        {"imageLabels", ComponentType::IMAGE_LABEL},
        {"canvases", ComponentType::CANVAS},
        {"checkboxes", ComponentType::CHECKBOX},
        {"dropdowns", ComponentType::DROPDOWN},
        {"tabBars", ComponentType::TAB_BAR},
        {"sliders", ComponentType::SLIDER},
        {"radioButtons", ComponentType::RADIO_BUTTON},
    };
    return names;
}

} // namespace Amethyst
