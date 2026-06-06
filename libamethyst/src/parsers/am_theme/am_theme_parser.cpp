#include "parsers/am_theme/am_theme_parser.h"
#include "logging/log.h"
#include "modules/style.h"
#include "modules/style_properties.def"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <toml++/toml.hpp>
#include <unordered_map>

namespace Amethyst {

static Color3 s_parseColor3(const toml::node &node)
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

static Color4 s_parseColor4(const toml::node &node)
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

static BorderMode s_parseBorderMode(std::string_view str)
{
    if (str == "outline") return BorderMode::OUTLINE;
    if (str == "middle") return BorderMode::MIDDLE;
    if (str == "inset") return BorderMode::INSET;
    return BorderMode::OUTLINE;
}

static TextXAlignment s_parseTextXAlignment(std::string_view str)
{
    if (str == "left") return TextXAlignment::LEFT;
    if (str == "center") return TextXAlignment::CENTER;
    if (str == "right") return TextXAlignment::RIGHT;
    return TextXAlignment::LEFT;
}

static TextYAlignment s_parseTextYAlignment(std::string_view str)
{
    if (str == "top") return TextYAlignment::TOP;
    if (str == "center") return TextYAlignment::CENTER;
    if (str == "bottom") return TextYAlignment::BOTTOM;
    return TextYAlignment::TOP;
}

static UDim s_parseUDim(const toml::node &node)
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

template <class T>
static StyleValue s_parseValue(const toml::node &n, Style &style);

template <>
StyleValue s_parseValue<Color3>(const toml::node &n, Style &)
{
    return StyleValue(s_parseColor3(n));
}

template <>
StyleValue s_parseValue<Color4>(const toml::node &n, Style &)
{
    return StyleValue(s_parseColor4(n));
}

template <>
StyleValue s_parseValue<float>(const toml::node &n, Style &)
{
    if (auto val = n.as_floating_point()) {
        return StyleValue(static_cast<float>(**val));
    }
    if (auto val = n.as_integer()) {
        return StyleValue(static_cast<float>(**val));
    }
    return StyleValue(0.0f);
}

template <>
StyleValue s_parseValue<UDim>(const toml::node &n, Style &)
{
    return StyleValue(s_parseUDim(n));
}

template <>
StyleValue s_parseValue<BorderMode>(const toml::node &n, Style &)
{
    if (auto str = n.as_string()) {
        return StyleValue(s_parseBorderMode(**str));
    }
    return StyleValue(BorderMode::OUTLINE);
}

template <>
StyleValue s_parseValue<TextXAlignment>(const toml::node &n, Style &)
{
    if (auto str = n.as_string()) {
        return StyleValue(s_parseTextXAlignment(**str));
    }
    return StyleValue(TextXAlignment::LEFT);
}

template <>
StyleValue s_parseValue<TextYAlignment>(const toml::node &n, Style &)
{
    if (auto str = n.as_string()) {
        return StyleValue(s_parseTextYAlignment(**str));
    }
    return StyleValue(TextYAlignment::TOP);
}

template <>
StyleValue s_parseValue<FontHandle>(const toml::node &n, Style &style)
{
    return StyleValue(FontHandle{style.internFont(n.value_or<std::string>("default"))});
}

struct PropParser {
    StyleProperty prop;
    StyleValue (*parse)(const toml::node &, Style &);
};

static const std::unordered_map<std::string, PropParser> &s_propParsers()
{
    static const std::unordered_map<std::string, PropParser> m = {
#define X(PROP, key, Type, dflt) {#key, {StyleProperty::PROP, &s_parseValue<Type>}},
        AM_STYLE_PROPS(X)
#undef X
    };
    return m;
}

using PropSink = std::function<void(StyleProperty, StyleValue)>;

static void s_parseSpacing(const toml::node &n, const PropSink &sink, StyleProperty top, StyleProperty right, StyleProperty bottom,
                           StyleProperty left)
{
    if (n.as_floating_point() || n.as_integer() || n.as_table()) {
        UDim v = s_parseUDim(n);
        sink(top, StyleValue(v));
        sink(right, StyleValue(v));
        sink(bottom, StyleValue(v));
        sink(left, StyleValue(v));
    } else if (auto arr = n.as_array()) {
        if (arr->size() == 2) {
            UDim vertical = s_parseUDim((*arr)[0]);
            UDim horizontal = s_parseUDim((*arr)[1]);
            sink(top, StyleValue(vertical));
            sink(bottom, StyleValue(vertical));
            sink(left, StyleValue(horizontal));
            sink(right, StyleValue(horizontal));
        } else if (arr->size() == 4) {
            sink(top, StyleValue(s_parseUDim((*arr)[0])));
            sink(right, StyleValue(s_parseUDim((*arr)[1])));
            sink(bottom, StyleValue(s_parseUDim((*arr)[2])));
            sink(left, StyleValue(s_parseUDim((*arr)[3])));
        }
    }
}

static bool s_handlePropEntry(std::string_view key, const toml::node &val, const PropSink &sink, Style &style)
{
    if (key == "padding") {
        s_parseSpacing(val, sink, StyleProperty::PADDING_TOP, StyleProperty::PADDING_RIGHT, StyleProperty::PADDING_BOTTOM,
                       StyleProperty::PADDING_LEFT);
        return true;
    }
    if (key == "cellPadding") {
        s_parseSpacing(val, sink, StyleProperty::CELL_PADDING_TOP, StyleProperty::CELL_PADDING_RIGHT,
                       StyleProperty::CELL_PADDING_BOTTOM, StyleProperty::CELL_PADDING_LEFT);
        return true;
    }

    const auto &parsers = s_propParsers();
    auto it = parsers.find(std::string(key));
    if (it != parsers.end()) {
        sink(it->second.prop, it->second.parse(val, style));
        return true;
    }
    return false;
}

static void s_parseClassBlock(const toml::table &t, const PropSink &sink, Style &style)
{
    for (const auto &[k, v] : t) {
        if (!s_handlePropEntry(k.str(), v, sink, style)) {
            AM_LOG_WARN("Unknown style property: {}", k.str());
        }
    }
}

static std::optional<Style> s_parseToml(const toml::table &tbl)
{
    Style style;
    uint32_t order = 0;
    const auto &typeNames = Style::getComponentTypeNames();

    for (const auto &[key, value] : tbl) {
        std::string_view k = key.str();

        if (k == "metadata") {
            continue;
        }

        if (k == "class") {
            if (auto classes = value.as_table()) {
                for (const auto &[clsName, clsVal] : *classes) {
                    if (auto clsTbl = clsVal.as_table()) {
                        StyleKey tok = Style::classToken(clsName.str());
                        style.registerClassName(tok, clsName.str());
                        uint32_t o = order++;
                        s_parseClassBlock(
                            *clsTbl, [&](StyleProperty p, StyleValue v) { style.addClassValue(tok, o, p, v); }, style);
                    }
                }
            }
            continue;
        }

        auto typeIt = typeNames.find(std::string(k));
        if (typeIt == typeNames.end()) {
            AM_LOG_WARN("Unknown section in theme: {}", std::string(k));
            continue;
        }
        ComponentType type = typeIt->second;

        auto section = value.as_table();
        if (section == nullptr) {
            continue;
        }

        for (const auto &[k2, v2] : *section) {
            PropSink typeSink = [&](StyleProperty p, StyleValue v) { style.addTypeValue(type, p, v); };
            if (s_handlePropEntry(k2.str(), v2, typeSink, style)) {
                continue;
            }
            if (auto clsTbl = v2.as_table()) {
                StyleKey tok = Style::classToken(k2.str());
                style.registerClassName(tok, k2.str());
                uint32_t o = order++;
                s_parseClassBlock(
                    *clsTbl, [&](StyleProperty p, StyleValue v) { style.addTypeClassValue(type, tok, o, p, v); }, style);
            } else {
                AM_LOG_WARN("Unknown style property: {}", k2.str());
            }
        }
    }

    return style;
}

std::optional<Style> AmThemeParser::parseFile(const std::filesystem::path &path)
{
    toml::parse_result parseResult = toml::parse_file(path.string());
    if (!parseResult) {
        AM_LOG_ERROR("Failed to parse theme file {}", path.string());
        return std::nullopt;
    }

    toml::table &tbl = parseResult.table();
    auto result = s_parseToml(tbl);
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
}

std::optional<Style> AmThemeParser::parseString(const std::string &tomlContent)
{
    toml::parse_result parseResult = toml::parse(tomlContent);
    if (!parseResult) {
        AM_LOG_ERROR("Failed to parse theme string");
        return std::nullopt;
    }

    toml::table &tbl = parseResult.table();
    auto result = s_parseToml(tbl);
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
}

} // namespace Amethyst
