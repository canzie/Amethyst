#include "parsers/am_theme/am_theme_parser.h"
#include "logging/log.h"
#include "modules/style.h"
#include "modules/style_properties.def"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Amethyst {

static std::string_view s_trim(std::string_view sv)
{
    size_t b = sv.find_first_not_of(" \t\r\n\f");
    if (b == std::string_view::npos) {
        return {};
    }
    size_t e = sv.find_last_not_of(" \t\r\n\f");
    return sv.substr(b, e - b + 1);
}

static std::vector<std::string_view> s_split(std::string_view sv, char delim)
{
    std::vector<std::string_view> out;
    size_t start = 0;
    while (true) {
        size_t pos = sv.find(delim, start);
        if (pos == std::string_view::npos) {
            out.push_back(s_trim(sv.substr(start)));
            break;
        }
        out.push_back(s_trim(sv.substr(start, pos - start)));
        start = pos + 1;
    }
    return out;
}

static std::vector<std::string_view> s_splitWhitespace(std::string_view sv)
{
    std::vector<std::string_view> out;
    size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) {
            ++i;
        }
        size_t start = i;
        while (i < sv.size() && !std::isspace(static_cast<unsigned char>(sv[i]))) {
            ++i;
        }
        if (i > start) {
            out.push_back(sv.substr(start, i - start));
        }
    }
    return out;
}

static float s_parseNumber(std::string_view sv)
{
    std::string s(sv);
    return std::strtof(s.c_str(), nullptr);
}

static bool s_endsWith(std::string_view sv, std::string_view suffix)
{
    return sv.size() >= suffix.size() && sv.substr(sv.size() - suffix.size()) == suffix;
}

static uint32_t s_parseHexDigits(std::string_view hex)
{
    return static_cast<uint32_t>(std::strtoul(std::string(hex).c_str(), nullptr, 16));
}

static std::string s_expandShortHex(std::string_view hex)
{
    std::string out;
    out.reserve(hex.size() * 2);
    for (char c : hex) {
        out.push_back(c);
        out.push_back(c);
    }
    return out;
}

static bool s_parseRgbCall(std::string_view sv, std::array<int, 4> &out, int &count)
{
    size_t open = sv.find('(');
    size_t close = sv.find(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open) {
        return false;
    }
    std::vector<std::string_view> parts = s_split(sv.substr(open + 1, close - open - 1), ',');
    count = 0;
    for (std::string_view p : parts) {
        if (p.empty() || count >= 4) {
            continue;
        }
        out[count++] = static_cast<int>(std::strtol(std::string(p).c_str(), nullptr, 10));
    }
    return count >= 3;
}

static Color3 s_parseColor3(std::string_view sv)
{
    sv = s_trim(sv);
    if (!sv.empty() && sv[0] == '#') {
        std::string_view hex = sv.substr(1);
        std::string expanded;
        if (hex.size() == 3) {
            expanded = s_expandShortHex(hex);
            hex = expanded;
        }
        return Color3::fromHex(s_parseHexDigits(hex.substr(0, 6)));
    }
    std::array<int, 4> rgb{};
    int count = 0;
    if (s_parseRgbCall(sv, rgb, count)) {
        return Color3::fromRgb(static_cast<uint8_t>(rgb[0]), static_cast<uint8_t>(rgb[1]), static_cast<uint8_t>(rgb[2]));
    }
    AM_LOG_WARN("Invalid color3 value: {}", std::string(sv));
    return Color3(1.0f, 1.0f, 1.0f);
}

static Color4 s_parseColor4(std::string_view sv)
{
    sv = s_trim(sv);
    if (!sv.empty() && sv[0] == '#') {
        std::string_view hex = sv.substr(1);
        std::string expanded;
        if (hex.size() == 3) {
            expanded = s_expandShortHex(hex);
            hex = expanded;
        }
        bool hasAlpha = hex.size() >= 8;
        return Color4::fromHex(s_parseHexDigits(hex.substr(0, hasAlpha ? 8 : 6)), hasAlpha);
    }
    std::array<int, 4> rgb{};
    int count = 0;
    if (s_parseRgbCall(sv, rgb, count)) {
        uint8_t a = count >= 4 ? static_cast<uint8_t>(rgb[3]) : 255;
        return Color4::fromRgb(static_cast<uint8_t>(rgb[0]), static_cast<uint8_t>(rgb[1]), static_cast<uint8_t>(rgb[2]), a);
    }
    AM_LOG_WARN("Invalid color4 value: {}", std::string(sv));
    return Color4(0.0f, 0.0f, 0.0f, 1.0f);
}

static float s_parseLength(std::string_view sv)
{
    sv = s_trim(sv);
    if (s_endsWith(sv, "px")) {
        return s_parseNumber(sv.substr(0, sv.size() - 2));
    }
    AM_LOG_WARN("Length value missing 'px' unit: {}", std::string(sv));
    return s_parseNumber(sv);
}

static float s_parseRatio(std::string_view sv)
{
    sv = s_trim(sv);
    if (s_endsWith(sv, "px")) {
        AM_LOG_WARN("Ratio value should be unitless (0..1): {}", std::string(sv));
        return s_parseNumber(sv.substr(0, sv.size() - 2));
    }
    if (s_endsWith(sv, "%")) {
        return s_parseNumber(sv.substr(0, sv.size() - 1)) / 100.0f;
    }
    return s_parseNumber(sv);
}

static UDim s_parseUDim(std::string_view sv)
{
    float scale = 0.0f;
    float offset = 0.0f;
    bool sawUnit = false;
    for (std::string_view term : s_split(sv, '+')) {
        if (term.empty()) {
            continue;
        }
        if (s_endsWith(term, "%")) {
            scale += s_parseNumber(term.substr(0, term.size() - 1)) / 100.0f;
            sawUnit = true;
        } else if (s_endsWith(term, "px")) {
            offset += s_parseNumber(term.substr(0, term.size() - 2));
            sawUnit = true;
        } else {
            offset += s_parseNumber(term);
        }
    }
    if (!sawUnit) {
        AM_LOG_WARN("Dimension value missing 'px' or '%' unit: {}", std::string(sv));
    }
    return UDim{scale, offset};
}

static BorderMode s_parseBorderMode(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv == "outline") {
        return BorderMode::OUTLINE;
    }
    if (sv == "middle") {
        return BorderMode::MIDDLE;
    }
    if (sv == "inset") {
        return BorderMode::INSET;
    }
    return BorderMode::OUTLINE;
}

static TextXAlignment s_parseTextXAlignment(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv == "left") {
        return TextXAlignment::LEFT;
    }
    if (sv == "center") {
        return TextXAlignment::CENTER;
    }
    if (sv == "right") {
        return TextXAlignment::RIGHT;
    }
    return TextXAlignment::LEFT;
}

static TextYAlignment s_parseTextYAlignment(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv == "top") {
        return TextYAlignment::TOP;
    }
    if (sv == "center") {
        return TextYAlignment::CENTER;
    }
    if (sv == "bottom") {
        return TextYAlignment::BOTTOM;
    }
    return TextYAlignment::TOP;
}

static std::string_view s_unquote(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv.size() >= 2 && (sv.front() == '"' || sv.front() == '\'') && sv.back() == sv.front()) {
        return sv.substr(1, sv.size() - 2);
    }
    return sv;
}

static StyleValue s_parse_COLOR3(std::string_view sv, Style &)
{
    return StyleValue(s_parseColor3(sv));
}

static StyleValue s_parse_COLOR4(std::string_view sv, Style &)
{
    return StyleValue(s_parseColor4(sv));
}

static StyleValue s_parse_LENGTH(std::string_view sv, Style &)
{
    return StyleValue(s_parseLength(sv));
}

static StyleValue s_parse_RATIO(std::string_view sv, Style &)
{
    return StyleValue(s_parseRatio(sv));
}

static StyleValue s_parse_UDIM(std::string_view sv, Style &)
{
    return StyleValue(s_parseUDim(sv));
}

static StyleValue s_parse_BMODE(std::string_view sv, Style &)
{
    return StyleValue(s_parseBorderMode(sv));
}

static StyleValue s_parse_XALIGN(std::string_view sv, Style &)
{
    return StyleValue(s_parseTextXAlignment(sv));
}

static StyleValue s_parse_YALIGN(std::string_view sv, Style &)
{
    return StyleValue(s_parseTextYAlignment(sv));
}

static StyleValue s_parse_FONT(std::string_view sv, Style &style)
{
    return StyleValue(FontHandle{style.internFont(s_unquote(sv))});
}

struct PropParser {
    StyleProperty prop;
    StyleValue (*parse)(std::string_view, Style &);
};

static const std::unordered_map<std::string, PropParser> &s_propParsers()
{
    static const std::unordered_map<std::string, PropParser> m = {
#define X(PROP, key, Type, tag, dflt) {key, {StyleProperty::PROP, &s_parse_##tag}},
        AM_STYLE_PROPS(X)
#undef X
    };
    return m;
}

using Decl = std::pair<StyleProperty, StyleValue>;

static void s_parseSpacing(std::string_view value, std::vector<Decl> &out, StyleProperty top, StyleProperty right,
                           StyleProperty bottom, StyleProperty left)
{
    std::vector<std::string_view> tokens = s_splitWhitespace(value);
    if (tokens.size() == 1) {
        UDim v = s_parseUDim(tokens[0]);
        out.push_back({top, StyleValue(v)});
        out.push_back({right, StyleValue(v)});
        out.push_back({bottom, StyleValue(v)});
        out.push_back({left, StyleValue(v)});
    } else if (tokens.size() == 2) {
        UDim vertical = s_parseUDim(tokens[0]);
        UDim horizontal = s_parseUDim(tokens[1]);
        out.push_back({top, StyleValue(vertical)});
        out.push_back({bottom, StyleValue(vertical)});
        out.push_back({left, StyleValue(horizontal)});
        out.push_back({right, StyleValue(horizontal)});
    } else if (tokens.size() == 4) {
        out.push_back({top, StyleValue(s_parseUDim(tokens[0]))});
        out.push_back({right, StyleValue(s_parseUDim(tokens[1]))});
        out.push_back({bottom, StyleValue(s_parseUDim(tokens[2]))});
        out.push_back({left, StyleValue(s_parseUDim(tokens[3]))});
    } else {
        AM_LOG_WARN("Spacing shorthand expects 1, 2 or 4 values: {}", std::string(value));
    }
}

static void s_parseDeclaration(std::string_view key, std::string_view value, std::vector<Decl> &out, Style &style)
{
    if (key == "padding") {
        s_parseSpacing(value, out, StyleProperty::PADDING_TOP, StyleProperty::PADDING_RIGHT, StyleProperty::PADDING_BOTTOM,
                       StyleProperty::PADDING_LEFT);
        return;
    }
    if (key == "cell-padding") {
        s_parseSpacing(value, out, StyleProperty::CELL_PADDING_TOP, StyleProperty::CELL_PADDING_RIGHT,
                       StyleProperty::CELL_PADDING_BOTTOM, StyleProperty::CELL_PADDING_LEFT);
        return;
    }

    const auto &parsers = s_propParsers();
    auto it = parsers.find(std::string(key));
    if (it == parsers.end()) {
        AM_LOG_WARN("Unknown style property: {}", std::string(key));
        return;
    }
    out.push_back({it->second.prop, it->second.parse(value, style)});
}

static std::vector<Decl> s_parseBlock(std::string_view block, Style &style)
{
    std::vector<Decl> out;
    for (std::string_view stmt : s_split(block, ';')) {
        if (stmt.empty()) {
            continue;
        }
        size_t colon = stmt.find(':');
        if (colon == std::string_view::npos) {
            AM_LOG_WARN("Malformed declaration (no ':'): {}", std::string(stmt));
            continue;
        }
        std::string_view key = s_trim(stmt.substr(0, colon));
        std::string_view value = s_trim(stmt.substr(colon + 1));
        if (key.empty() || value.empty()) {
            continue;
        }
        s_parseDeclaration(key, value, out, style);
    }
    return out;
}

enum class SelectorKind {
    TYPE,
    CLASS,
    TYPE_CLASS,
    PART,
    INVALID
};

struct Selector {
    SelectorKind kind = SelectorKind::INVALID;
    ComponentType type = ComponentType::UI_OBJECT;
    StyleKey classToken = 0;
    std::string className;
    std::string_view pseudo;
};

static Selector s_parseSelector(std::string_view sel)
{
    Selector out;
    sel = s_trim(sel);

    size_t colon = sel.find(':');
    if (colon != std::string_view::npos) {
        out.pseudo = s_trim(sel.substr(colon + 1));
        sel = s_trim(sel.substr(0, colon));
    }
    if (sel.empty()) {
        return out;
    }

    const auto &typeNames = Style::getComponentTypeNames();

    if (sel[0] == '.') {
        std::string name(s_trim(sel.substr(1)));
        out.kind = SelectorKind::CLASS;
        out.classToken = Style::classToken(name);
        out.className = name;
        return out;
    }

    size_t hash = sel.find('#');
    if (hash != std::string_view::npos) {
        std::string_view typePart = s_trim(sel.substr(0, hash));
        auto typeIt = typeNames.find(std::string(typePart));
        if (typeIt == typeNames.end()) {
            AM_LOG_WARN("Unknown component type in selector: {}", std::string(typePart));
            return out;
        }
        out.kind = SelectorKind::PART;
        out.type = typeIt->second;
        out.classToken = Style::classToken(sel);
        out.className = std::string(sel);
        return out;
    }

    size_t dot = sel.find('.');
    if (dot != std::string_view::npos) {
        std::string_view typePart = s_trim(sel.substr(0, dot));
        std::string name(s_trim(sel.substr(dot + 1)));
        auto typeIt = typeNames.find(std::string(typePart));
        if (typeIt == typeNames.end()) {
            AM_LOG_WARN("Unknown component type in selector: {}", std::string(typePart));
            return out;
        }
        out.kind = SelectorKind::TYPE_CLASS;
        out.type = typeIt->second;
        out.classToken = Style::classToken(name);
        out.className = name;
        return out;
    }

    auto typeIt = typeNames.find(std::string(sel));
    if (typeIt == typeNames.end()) {
        AM_LOG_WARN("Unknown component type in selector: {}", std::string(sel));
        return out;
    }
    out.kind = SelectorKind::TYPE;
    out.type = typeIt->second;
    return out;
}

static void s_applySelector(const Selector &sel, const std::vector<Decl> &decls, Style &style, uint32_t &order)
{
    if (!sel.pseudo.empty()) {
        AM_LOG_DEBUG("Pseudo-state '{}' is not yet supported; skipping rule", std::string(sel.pseudo));
        return;
    }

    switch (sel.kind) {
    case SelectorKind::TYPE:
        for (const auto &[prop, value] : decls) {
            style.addTypeValue(sel.type, prop, value);
        }
        break;
    case SelectorKind::CLASS:
    case SelectorKind::PART: {
        style.registerClassName(sel.classToken, sel.className);
        uint32_t o = order++;
        for (const auto &[prop, value] : decls) {
            style.addClassValue(sel.classToken, o, prop, value);
        }
        break;
    }
    case SelectorKind::TYPE_CLASS: {
        style.registerClassName(sel.classToken, sel.className);
        uint32_t o = order++;
        for (const auto &[prop, value] : decls) {
            style.addTypeClassValue(sel.type, sel.classToken, o, prop, value);
        }
        break;
    }
    case SelectorKind::INVALID:
        break;
    }
}

static std::string s_stripComments(std::string_view src)
{
    std::string out;
    out.reserve(src.size());
    for (size_t i = 0; i < src.size();) {
        if (i + 1 < src.size() && src[i] == '/' && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) {
                ++i;
            }
            i += 2;
            out.push_back(' ');
        } else {
            out.push_back(src[i]);
            ++i;
        }
    }
    return out;
}

static Style s_parseSource(std::string_view rawSource)
{
    std::string source = s_stripComments(rawSource);
    std::string_view src = source;

    Style style;
    uint32_t order = 0;
    size_t pos = 0;

    while (true) {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            ++pos;
        }
        if (pos >= src.size()) {
            break;
        }

        size_t braceOpen = src.find('{', pos);
        if (braceOpen == std::string_view::npos) {
            AM_LOG_WARN("Trailing content with no rule body");
            break;
        }
        size_t braceClose = src.find('}', braceOpen + 1);
        if (braceClose == std::string_view::npos) {
            AM_LOG_WARN("Unterminated rule body");
            break;
        }

        std::string_view selectorList = src.substr(pos, braceOpen - pos);
        std::string_view block = src.substr(braceOpen + 1, braceClose - braceOpen - 1);
        pos = braceClose + 1;

        std::vector<Decl> decls = s_parseBlock(block, style);
        for (std::string_view selText : s_split(selectorList, ',')) {
            if (selText.empty()) {
                continue;
            }
            Selector sel = s_parseSelector(selText);
            s_applySelector(sel, decls, style, order);
        }
    }

    return style;
}

std::optional<Style> AmThemeParser::parseFile(const std::filesystem::path &path)
{
    std::ifstream file(path);
    if (!file) {
        AM_LOG_ERROR("Failed to open theme file {}", path.string());
        return std::nullopt;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    Style style = s_parseSource(buffer.str());
    AM_LOG_INFO("Loaded theme from {}", path.string());
    return style;
}

std::optional<Style> AmThemeParser::parseString(const std::string &source)
{
    Style style = s_parseSource(source);
    AM_LOG_INFO("Loaded theme from string");
    return style;
}

} // namespace Amethyst
