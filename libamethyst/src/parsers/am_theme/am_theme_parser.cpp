#include "parsers/am_theme/am_theme_parser.h"
#include "logging/log.h"

#include <string_view>
#include <toml++/toml.hpp>

namespace Amethyst {

namespace {

Color3 parseColor3(const toml::node &node)
{
    if (auto str = node.as_string()) {
        std::string_view hex = **str;
        if (!hex.empty() && hex[0] == '#') {
            hex = hex.substr(1);
        }
        uint32_t value = std::stoul(std::string(hex), nullptr, 16);
        return Color3::fromHex(value);
    }
    if (auto arr = node.as_array()) {
        if (arr->size() >= 3) {
            return Color3::fromRgb(static_cast<uint8_t>((*arr)[0].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[1].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[2].value<int64_t>().value_or(0)));
        }
    }
    return Color3(1.0f, 1.0f, 1.0f);
}

Color4 parseColor4(const toml::node &node)
{
    if (auto str = node.as_string()) {
        std::string_view hex = **str;
        if (!hex.empty() && hex[0] == '#') {
            hex = hex.substr(1);
        }
        uint32_t value = std::stoul(std::string(hex), nullptr, 16);
        bool hasAlpha = hex.length() > 6;
        return Color4::fromHex(value, hasAlpha);
    }
    if (auto arr = node.as_array()) {
        if (arr->size() >= 4) {
            return Color4::fromRgb(static_cast<uint8_t>((*arr)[0].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[1].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[2].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[3].value<int64_t>().value_or(0)));
        }
        if (arr->size() >= 3) {
            return Color4::fromRgb(static_cast<uint8_t>((*arr)[0].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[1].value<int64_t>().value_or(0)),
                                   static_cast<uint8_t>((*arr)[2].value<int64_t>().value_or(0)));
        }
    }
    return Color4(0.0f, 0.0f, 0.0f, 1.0f);
}

BorderMode parseBorderMode(std::string_view str)
{
    if (str == "outline") return BorderMode::OUTLINE;
    if (str == "middle") return BorderMode::MIDDLE;
    if (str == "inset") return BorderMode::INSET;
    return BorderMode::OUTLINE;
}

TextXAlignment parseTextXAlignment(std::string_view str)
{
    if (str == "left") return TextXAlignment::LEFT;
    if (str == "center") return TextXAlignment::CENTER;
    if (str == "right") return TextXAlignment::RIGHT;
    return TextXAlignment::LEFT;
}

TextYAlignment parseTextYAlignment(std::string_view str)
{
    if (str == "top") return TextYAlignment::TOP;
    if (str == "center") return TextYAlignment::CENTER;
    if (str == "bottom") return TextYAlignment::BOTTOM;
    return TextYAlignment::TOP;
}

UDim parseUDim(const toml::node &node)
{
    if (auto val = node.as_floating_point()) {
        return UDim::fromOffset(static_cast<float>(**val));
    }
    if (auto val = node.as_integer()) {
        return UDim::fromOffset(static_cast<float>(**val));
    }
    if (auto tbl = node.as_table()) {
        float scale = (*tbl)["scale"].value<double>().value_or(0.0);
        float offset = (*tbl)["offset"].value<double>().value_or(0.0);
        return UDim{scale, offset};
    }
    return UDim{};
}

void parseSpacingShorthand(const toml::node &node, Style &style, ComponentType type, StyleProperty top, StyleProperty right,
                           StyleProperty bottom, StyleProperty left)
{
    if (node.as_floating_point() || node.as_integer() || node.as_table()) {
        UDim v = parseUDim(node);
        style.set(top, type, v);
        style.set(right, type, v);
        style.set(bottom, type, v);
        style.set(left, type, v);
    } else if (auto arr = node.as_array()) {
        if (arr->size() == 2) {
            UDim vertical = parseUDim((*arr)[0]);
            UDim horizontal = parseUDim((*arr)[1]);
            style.set(top, type, vertical);
            style.set(bottom, type, vertical);
            style.set(left, type, horizontal);
            style.set(right, type, horizontal);
        } else if (arr->size() == 4) {
            style.set(top, type, parseUDim((*arr)[0]));
            style.set(right, type, parseUDim((*arr)[1]));
            style.set(bottom, type, parseUDim((*arr)[2]));
            style.set(left, type, parseUDim((*arr)[3]));
        }
    }
}

void parseSection(const toml::table &section, Style &style, ComponentType type)
{
    const auto &propNames = Style::getPropertyNames();

    for (const auto &[key, value] : section) {
        std::string_view keyStr = key.str();

        if (keyStr == "padding") {
            parseSpacingShorthand(value, style, type, StyleProperty::PADDING_TOP, StyleProperty::PADDING_RIGHT,
                                  StyleProperty::PADDING_BOTTOM, StyleProperty::PADDING_LEFT);
            continue;
        }
        if (keyStr == "cellPadding") {
            parseSpacingShorthand(value, style, type, StyleProperty::CELL_PADDING_TOP, StyleProperty::CELL_PADDING_RIGHT,
                                  StyleProperty::CELL_PADDING_BOTTOM, StyleProperty::CELL_PADDING_LEFT);
            continue;
        }

        auto propIt = propNames.find(std::string(keyStr));
        if (propIt == propNames.end()) {
            AM_LOG_WARN("Unknown style property: {}", std::string(keyStr));
            continue;
        }

        StyleProperty prop = propIt->second;

        switch (prop) {
        case StyleProperty::BACKGROUND_COLOR:
        case StyleProperty::BORDER_COLOR:
        case StyleProperty::SCROLLBAR_COLOR:
        case StyleProperty::SCROLLBAR_THUMB_COLOR:
        case StyleProperty::SLIDER_COLOR:
        case StyleProperty::THUMB_COLOR:
        case StyleProperty::CHECK_COLOR:
        case StyleProperty::HIGHLIGHT_COLOR:
            style.set(prop, type, parseColor3(value));
            break;

        case StyleProperty::TEXT_COLOR:
        case StyleProperty::STROKE_COLOR:
        case StyleProperty::COLUMN_SEPARATOR_COLOR:
        case StyleProperty::ROW_BACKGROUND_COLOR:
        case StyleProperty::ROW_ALTERNATE_COLOR:
        case StyleProperty::ROW_HOVER_COLOR:
        case StyleProperty::ROW_SELECTED_COLOR:
        case StyleProperty::DISCLOSURE_TRIANGLE_COLOR:
        case StyleProperty::LABEL_COLOR:
        case StyleProperty::VALUE_COLOR:
            style.set(prop, type, parseColor4(value));
            break;

        case StyleProperty::BORDER_MODE:
            if (auto str = value.as_string()) {
                style.set(prop, type, parseBorderMode(**str));
            }
            break;

        case StyleProperty::TEXT_X_ALIGNMENT:
            if (auto str = value.as_string()) {
                style.set(prop, type, parseTextXAlignment(**str));
            }
            break;

        case StyleProperty::TEXT_Y_ALIGNMENT:
            if (auto str = value.as_string()) {
                style.set(prop, type, parseTextYAlignment(**str));
            }
            break;

        case StyleProperty::FONT_FAMILY:
            if (auto str = value.as_string()) {
                style.set(prop, type, std::string(**str));
            }
            break;

        case StyleProperty::PADDING_TOP:
        case StyleProperty::PADDING_RIGHT:
        case StyleProperty::PADDING_BOTTOM:
        case StyleProperty::PADDING_LEFT:
        case StyleProperty::CELL_PADDING_TOP:
        case StyleProperty::CELL_PADDING_RIGHT:
        case StyleProperty::CELL_PADDING_BOTTOM:
        case StyleProperty::CELL_PADDING_LEFT:
        case StyleProperty::LABEL_PADDING:
            style.set(prop, type, parseUDim(value));
            break;

        default:
            if (auto val = value.as_floating_point()) {
                style.set(prop, type, static_cast<float>(**val));
            } else if (auto val = value.as_integer()) {
                style.set(prop, type, static_cast<float>(**val));
            }
            break;
        }
    }
}

std::optional<Style> parseToml(const toml::table &tbl)
{
    Style style;
    const auto &typeNames = Style::getComponentTypeNames();

    for (const auto &[key, value] : tbl) {
        std::string_view keyStr = key.str();

        if (keyStr == "metadata") continue;

        auto typeIt = typeNames.find(std::string(keyStr));
        if (typeIt == typeNames.end()) {
            AM_LOG_WARN("Unknown section in theme: {}", std::string(keyStr));
            continue;
        }

        if (auto section = value.as_table()) {
            parseSection(*section, style, typeIt->second);
        }
    }

    return style;
}

} // anonymous namespace

std::optional<Style> AmThemeParser::parseFile(const std::filesystem::path &path)
{
    try {
        toml::table tbl = toml::parse_file(path.string());
        auto result = parseToml(tbl);
        if (result) {
            std::string themeName = "unknown";
            if (auto meta = tbl["metadata"].as_table()) {
                if (auto name = (*meta)["name"].as_string()) {
                    themeName = **name;
                }
            }
            AM_LOG_INFO("Loaded theme '{}' from {}", themeName, path.string());
        }
        return result;
    } catch (const toml::parse_error &err) {
        AM_LOG_ERROR("Failed to parse theme file {}: {}", path.string(), err.what());
        return std::nullopt;
    }
}

std::optional<Style> AmThemeParser::parseString(const std::string &tomlContent)
{
    try {
        toml::table tbl = toml::parse(tomlContent);
        auto result = parseToml(tbl);
        if (result) {
            std::string themeName = "unknown";
            if (auto meta = tbl["metadata"].as_table()) {
                if (auto name = (*meta)["name"].as_string()) {
                    themeName = **name;
                }
            }
            AM_LOG_INFO("Loaded theme '{}'", themeName);
        }
        return result;
    } catch (const toml::parse_error &err) {
        AM_LOG_ERROR("Failed to parse theme string: {}", err.what());
        return std::nullopt;
    }
}

} // namespace Amethyst
