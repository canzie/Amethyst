#include "parsers/am_theme/am_theme_parser.h"
#include "logging/log.h"
#include "modules/style.h"
#include "modules/style_properties.def"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
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

// Every value parser returns nullopt on malformed input after logging why; the declaration is then
// dropped, CSS-style, so a typo keeps the cascaded value instead of silently painting black.

static std::optional<float> s_parseNumber(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv.empty()) {
        return std::nullopt;
    }
    std::string s(sv);
    char *end = nullptr;
    errno = 0;
    float v = std::strtof(s.c_str(), &end);
    if (end != s.c_str() + s.size() || errno == ERANGE || !std::isfinite(v)) {
        return std::nullopt;
    }
    return v;
}

static bool s_endsWith(std::string_view sv, std::string_view suffix)
{
    return sv.size() >= suffix.size() && sv.substr(sv.size() - suffix.size()) == suffix;
}

static bool s_isHexDigits(std::string_view hex)
{
    return !hex.empty() &&
           std::all_of(hex.begin(), hex.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)) != 0; });
}

/**
 * @brief Parse #rgb, #rgba, #rrggbb or #rrggbbaa into normalized channels.
 * @param sv The value, starting at '#'
 * @param out Receives r, g, b, a in [0, 1]; a is 1 when the value carries no alpha
 * @return False (after logging) on a bad digit or length
 */
static bool s_parseHexColor(std::string_view sv, std::array<float, 4> &out)
{
    std::string_view hex = sv.substr(1);
    if (!s_isHexDigits(hex) || (hex.size() != 3 && hex.size() != 4 && hex.size() != 6 && hex.size() != 8)) {
        AM_LOG_WARN("Invalid hex color '{}': expected #rgb, #rgba, #rrggbb or #rrggbbaa", std::string(sv));
        return false;
    }
    std::string expanded;
    if (hex.size() <= 4) {
        for (char c : hex) {
            expanded.push_back(c);
            expanded.push_back(c);
        }
        hex = expanded;
    }
    uint32_t bits = static_cast<uint32_t>(std::strtoul(std::string(hex).c_str(), nullptr, 16));
    if (hex.size() == 6) {
        bits = (bits << 8) | 0xFFu;
    }
    out = {((bits >> 24) & 0xFF) / 255.0f, ((bits >> 16) & 0xFF) / 255.0f, ((bits >> 8) & 0xFF) / 255.0f, (bits & 0xFF) / 255.0f};
    return true;
}

/**
 * @brief Parse rgb(r, g, b) / rgba(r, g, b, a). Channels are 0..255; alpha is CSS's 0..1 (or a
 * percentage), and a whole number above 1 is read as the legacy 0..255 form the bundled themes use.
 * @param sv The value text
 * @param out Receives r, g, b, a in [0, 1]
 * @return False (after logging) when the call is malformed
 */
static bool s_parseRgbCall(std::string_view sv, std::array<float, 4> &out)
{
    bool hasAlpha = sv.starts_with("rgba(");
    if (!hasAlpha && !sv.starts_with("rgb(")) {
        return false;
    }
    if (!s_endsWith(sv, ")")) {
        AM_LOG_WARN("Unterminated color call '{}'", std::string(sv));
        return false;
    }
    size_t open = sv.find('(');
    std::vector<std::string_view> parts = s_split(sv.substr(open + 1, sv.size() - open - 2), ',');
    size_t expected = hasAlpha ? 4 : 3;
    if (parts.size() != expected) {
        AM_LOG_WARN("Color call '{}' expects {} components, got {}", std::string(sv), expected, parts.size());
        return false;
    }
    for (size_t i = 0; i < 3; ++i) {
        std::optional<float> channel = s_parseNumber(parts[i]);
        if (!channel.has_value()) {
            AM_LOG_WARN("Invalid color channel '{}' in '{}'", std::string(parts[i]), std::string(sv));
            return false;
        }
        if (*channel < 0.0f || *channel > 255.0f) {
            AM_LOG_WARN("Color channel {} out of 0..255 in '{}'; clamping", *channel, std::string(sv));
        }
        out[i] = std::clamp(std::round(*channel), 0.0f, 255.0f) / 255.0f;
    }
    out[3] = 1.0f;
    if (hasAlpha) {
        std::string_view alphaText = parts[3];
        bool percent = s_endsWith(alphaText, "%");
        std::optional<float> alpha = s_parseNumber(percent ? alphaText.substr(0, alphaText.size() - 1) : alphaText);
        if (!alpha.has_value()) {
            AM_LOG_WARN("Invalid alpha '{}' in '{}'", std::string(alphaText), std::string(sv));
            return false;
        }
        float a = *alpha;
        if (percent) {
            a /= 100.0f;
        } else if (a > 1.0f && alphaText.find('.') == std::string_view::npos) {
            a /= 255.0f;
        }
        if (a < 0.0f || a > 1.0f) {
            AM_LOG_WARN("Alpha out of range in '{}'; clamping", std::string(sv));
        }
        out[3] = std::clamp(a, 0.0f, 1.0f);
    }
    return true;
}

static bool s_parseColorChannels(std::string_view sv, std::array<float, 4> &out)
{
    sv = s_trim(sv);
    if (!sv.empty() && sv[0] == '#') {
        return s_parseHexColor(sv, out);
    }
    if (s_parseRgbCall(sv, out)) {
        return true;
    }
    if (!sv.starts_with("rgb")) {
        AM_LOG_WARN("Invalid color value: {}", std::string(sv));
    }
    return false;
}

static std::optional<Color3> s_parseColor3(std::string_view sv)
{
    std::array<float, 4> c{};
    if (!s_parseColorChannels(sv, c)) {
        return std::nullopt;
    }
    if (c[3] != 1.0f) {
        AM_LOG_WARN("Alpha ignored on an opaque color property: {}", std::string(s_trim(sv)));
    }
    return Color3(c[0], c[1], c[2]);
}

static std::optional<Color4> s_parseColor4(std::string_view sv)
{
    std::array<float, 4> c{};
    if (!s_parseColorChannels(sv, c)) {
        return std::nullopt;
    }
    return Color4(c[0], c[1], c[2], c[3]);
}

static std::optional<float> s_parseLength(std::string_view sv)
{
    sv = s_trim(sv);
    std::optional<float> v;
    if (s_endsWith(sv, "px")) {
        v = s_parseNumber(sv.substr(0, sv.size() - 2));
    } else {
        v = s_parseNumber(sv);
        if (v.has_value() && *v != 0.0f) {
            AM_LOG_WARN("Length value missing 'px' unit: {}", std::string(sv));
        }
    }
    if (!v.has_value()) {
        AM_LOG_WARN("Invalid length: {}", std::string(sv));
    }
    return v;
}

static std::optional<float> s_parseRatio(std::string_view sv)
{
    sv = s_trim(sv);
    std::optional<float> v;
    if (s_endsWith(sv, "px")) {
        AM_LOG_WARN("Ratio value should be unitless (0..1): {}", std::string(sv));
        v = s_parseNumber(sv.substr(0, sv.size() - 2));
    } else if (s_endsWith(sv, "%")) {
        v = s_parseNumber(sv.substr(0, sv.size() - 1));
        if (v.has_value()) {
            *v /= 100.0f;
        }
    } else {
        v = s_parseNumber(sv);
    }
    if (!v.has_value()) {
        AM_LOG_WARN("Invalid ratio: {}", std::string(sv));
    }
    return v;
}

static std::optional<UDim> s_parseUDim(std::string_view sv)
{
    float scale = 0.0f;
    float offset = 0.0f;
    bool sawUnit = false;
    for (std::string_view term : s_split(sv, '+')) {
        std::optional<float> v;
        if (s_endsWith(term, "%")) {
            v = s_parseNumber(term.substr(0, term.size() - 1));
            if (v.has_value()) {
                scale += *v / 100.0f;
            }
            sawUnit = true;
        } else if (s_endsWith(term, "px")) {
            v = s_parseNumber(term.substr(0, term.size() - 2));
            if (v.has_value()) {
                offset += *v;
            }
            sawUnit = true;
        } else {
            v = s_parseNumber(term);
            if (v.has_value()) {
                offset += *v;
            }
        }
        if (!v.has_value()) {
            AM_LOG_WARN("Invalid dimension term '{}' in: {}", std::string(term), std::string(sv));
            return std::nullopt;
        }
    }
    if (!sawUnit && offset != 0.0f) {
        AM_LOG_WARN("Dimension value missing 'px' or '%' unit: {}", std::string(sv));
    }
    return UDim{scale, offset};
}

/**
 * @brief Look a keyword up in a fixed table, warning when it isn't one of them.
 */
template <typename T, size_t N>
static std::optional<T> s_parseKeyword(std::string_view sv, const std::array<std::pair<std::string_view, T>, N> &table,
                                       std::string_view what)
{
    sv = s_trim(sv);
    for (const auto &[name, value] : table) {
        if (sv == name) {
            return value;
        }
    }
    AM_LOG_WARN("Unknown {} '{}'", what, std::string(sv));
    return std::nullopt;
}

static std::string_view s_unquote(std::string_view sv)
{
    sv = s_trim(sv);
    if (sv.size() >= 2 && (sv.front() == '"' || sv.front() == '\'') && sv.back() == sv.front()) {
        return sv.substr(1, sv.size() - 2);
    }
    return sv;
}

using ParsedValue = std::optional<StyleValue>;

template <typename T> static ParsedValue s_wrap(const std::optional<T> &v)
{
    if (!v.has_value()) {
        return std::nullopt;
    }
    return StyleValue(*v);
}

static ParsedValue s_parse_COLOR3(std::string_view sv, Style &)
{
    return s_wrap(s_parseColor3(sv));
}

static ParsedValue s_parse_COLOR4(std::string_view sv, Style &)
{
    return s_wrap(s_parseColor4(sv));
}

static ParsedValue s_parse_LENGTH(std::string_view sv, Style &)
{
    return s_wrap(s_parseLength(sv));
}

// corner radii are packed as unsigned pixels, so a negative or absurd length is clamped while it is still a float
static std::optional<uint32_t> s_lengthToRadius(std::string_view sv)
{
    std::optional<float> v = s_parseLength(sv);
    if (!v.has_value()) {
        return std::nullopt;
    }
    constexpr float MAX_RADIUS = 65535.0f;
    if (*v < 0.0f || *v > MAX_RADIUS) {
        AM_LOG_WARN("Corner radius {} out of range; clamping to 0..{}", *v, MAX_RADIUS);
    }
    return static_cast<uint32_t>(std::clamp(*v, 0.0f, MAX_RADIUS) + 0.5f);
}

// Corner shorthand follows CSS border-radius ordering (top-left, top-right, bottom-right, bottom-left):
// one value sets all four, two set the tl/br and tr/bl diagonals, four set each corner.
static ParsedValue s_parse_CORNERS(std::string_view sv, Style &)
{
    std::vector<std::string_view> tokens = s_splitWhitespace(sv);
    if (tokens.size() != 1 && tokens.size() != 2 && tokens.size() != 4) {
        AM_LOG_WARN("Corner-radius shorthand expects 1, 2 or 4 values: {}", std::string(sv));
        return std::nullopt;
    }
    std::array<uint32_t, 4> r{};
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::optional<uint32_t> v = s_lengthToRadius(tokens[i]);
        if (!v.has_value()) {
            return std::nullopt;
        }
        r[i] = *v;
    }
    if (tokens.size() == 1) {
        return StyleValue(uvec4(r[0]));
    }
    if (tokens.size() == 2) {
        return StyleValue(uvec4(r[0], r[1], r[0], r[1]));
    }
    return StyleValue(uvec4(r[0], r[1], r[2], r[3]));
}

static ParsedValue s_parse_RATIO(std::string_view sv, Style &)
{
    return s_wrap(s_parseRatio(sv));
}

static ParsedValue s_parse_UDIM(std::string_view sv, Style &)
{
    return s_wrap(s_parseUDim(sv));
}

static ParsedValue s_parse_BMODE(std::string_view sv, Style &)
{
    static constexpr std::array<std::pair<std::string_view, BorderMode>, 3> table = {{
        {"outline", BorderMode::OUTLINE},
        {"middle", BorderMode::MIDDLE},
        {"inset", BorderMode::INSET},
    }};
    return s_wrap(s_parseKeyword(sv, table, "border-mode"));
}

static ParsedValue s_parse_XALIGN(std::string_view sv, Style &)
{
    static constexpr std::array<std::pair<std::string_view, TextXAlignment>, 3> table = {{
        {"left", TextXAlignment::LEFT},
        {"center", TextXAlignment::CENTER},
        {"right", TextXAlignment::RIGHT},
    }};
    return s_wrap(s_parseKeyword(sv, table, "text-x-alignment"));
}

static ParsedValue s_parse_YALIGN(std::string_view sv, Style &)
{
    static constexpr std::array<std::pair<std::string_view, TextYAlignment>, 3> table = {{
        {"top", TextYAlignment::TOP},
        {"center", TextYAlignment::CENTER},
        {"bottom", TextYAlignment::BOTTOM},
    }};
    return s_wrap(s_parseKeyword(sv, table, "text-y-alignment"));
}

static ParsedValue s_parse_FONT(std::string_view sv, Style &style)
{
    return StyleValue(FontHandle{style.internFont(s_unquote(sv))});
}

static ParsedValue s_parse_ISCALE(std::string_view sv, Style &)
{
    static constexpr std::array<std::pair<std::string_view, ImageScaleType>, 4> table = {{
        {"stretch", ImageScaleType::STRETCH},
        {"tile", ImageScaleType::TILE},
        {"fit", ImageScaleType::FIT},
        {"crop", ImageScaleType::CROP},
    }};
    return s_wrap(s_parseKeyword(sv, table, "image-scale-type"));
}

static ParsedValue s_parse_BREPEAT(std::string_view sv, Style &)
{
    static constexpr std::array<std::pair<std::string_view, BackgroundRepeat>, 4> table = {{
        {"none", BackgroundRepeat::NONE},
        {"repeat", BackgroundRepeat::REPEAT},
        {"repeat-x", BackgroundRepeat::REPEAT_X},
        {"repeat-y", BackgroundRepeat::REPEAT_Y},
    }};
    return s_wrap(s_parseKeyword(sv, table, "background-repeat"));
}

static ParsedValue s_parse_SIZE2(std::string_view sv, Style &)
{
    std::vector<std::string_view> tokens = s_splitWhitespace(sv);
    if (tokens.size() != 1 && tokens.size() != 2) {
        AM_LOG_WARN("Size value expects 1 or 2 length values: {}", std::string(sv));
        return std::nullopt;
    }
    std::optional<float> x = s_parseLength(tokens[0]);
    std::optional<float> y = tokens.size() == 2 ? s_parseLength(tokens[1]) : x;
    if (!x.has_value() || !y.has_value()) {
        return std::nullopt;
    }
    return StyleValue(vec2(*x, *y));
}

static ParsedValue s_parse_NUMBER(std::string_view sv, Style &)
{
    std::optional<float> v = s_parseNumber(sv);
    if (!v.has_value()) {
        AM_LOG_WARN("Invalid number: {}", std::string(s_trim(sv)));
    }
    return s_wrap(v);
}

struct PropParser {
    StyleProperty prop;
    ParsedValue (*parse)(std::string_view, Style &);
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
using VarMap = std::unordered_map<std::string, std::string>;

static bool s_isVariableNameChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}

static std::optional<std::string> s_substituteVars(std::string_view text, const VarMap &vars)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        if (text[i] != '@') {
            out.push_back(text[i]);
            ++i;
            continue;
        }
        size_t j = i + 1;
        while (j < text.size() && s_isVariableNameChar(text[j])) {
            ++j;
        }
        std::string name(text.substr(i + 1, j - i - 1));
        auto it = vars.find(name);
        if (it == vars.end()) {
            AM_LOG_WARN("Undefined style variable '@{}'; skipping declaration", name);
            return std::nullopt;
        }
        out += it->second;
        i = j;
    }
    return out;
}

static void s_parseSpacing(std::string_view value, std::vector<Decl> &out, StyleProperty top, StyleProperty right,
                           StyleProperty bottom, StyleProperty left)
{
    std::vector<std::string_view> tokens = s_splitWhitespace(value);
    if (tokens.size() != 1 && tokens.size() != 2 && tokens.size() != 4) {
        AM_LOG_WARN("Spacing shorthand expects 1, 2 or 4 values: {}", std::string(value));
        return;
    }
    std::array<UDim, 4> v{};
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::optional<UDim> d = s_parseUDim(tokens[i]);
        if (!d.has_value()) {
            return;
        }
        v[i] = *d;
    }
    if (tokens.size() == 1) {
        v = {v[0], v[0], v[0], v[0]};
    } else if (tokens.size() == 2) {
        v = {v[0], v[1], v[0], v[1]};
    }
    out.push_back({top, StyleValue(v[0])});
    out.push_back({right, StyleValue(v[1])});
    out.push_back({bottom, StyleValue(v[2])});
    out.push_back({left, StyleValue(v[3])});
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
    ParsedValue parsed = it->second.parse(value, style);
    if (!parsed.has_value()) {
        AM_LOG_WARN("Dropping declaration '{}: {}'", std::string(key), std::string(value));
        return;
    }
    out.push_back({it->second.prop, std::move(*parsed)});
}

static std::vector<Decl> s_parseBlock(std::string_view block, Style &style, const VarMap &vars)
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
        std::optional<std::string> value = s_substituteVars(s_trim(stmt.substr(colon + 1)), vars);
        if (key.empty() || !value.has_value()) {
            continue;
        }
        std::string_view resolved = s_trim(*value);
        if (resolved.empty()) {
            continue;
        }
        s_parseDeclaration(key, resolved, out, style);
    }
    return out;
}

/**
 * @brief Normalized form of one parsed simple selector: `type? .class? #part? :pseudo?`, scanned
 * left to right. Comma-separated lists are split into simple selectors before this stage.
 */
struct SelectorSpec {
    StyleSelector selector;
    std::string className;
    bool valid = false;
};

static bool s_parsePseudoState(std::string_view name, uint16_t &out)
{
    if (name == "hover") {
        out = GUI_STATE_HOVERED;
    } else if (name == "pressed") {
        out = GUI_STATE_PRESSED;
    } else if (name == "disabled") {
        out = GUI_STATE_DISABLED;
    } else if (name == "focused") {
        out = GUI_STATE_FOCUSED;
    } else if (name == "active") {
        out = GUI_STATE_ACTIVE;
    } else {
        return false;
    }
    return true;
}

static SelectorSpec s_parseSelector(std::string_view sel)
{
    SelectorSpec out;
    sel = s_trim(sel);
    if (sel.empty()) {
        AM_LOG_WARN("Empty selector");
        return out;
    }
    if (std::any_of(sel.begin(), sel.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; })) {
        AM_LOG_WARN("Selector contains whitespace; descendant combinators are not supported: {}", std::string(sel));
        return out;
    }

    const auto &typeNames = Style::getComponentTypeNames();

    size_t typeEnd = sel.find_first_of(".#:");
    std::string_view typePart = sel.substr(0, typeEnd);
    size_t i = typeEnd == std::string_view::npos ? sel.size() : typeEnd;

    if (!typePart.empty()) {
        auto typeIt = typeNames.find(std::string(typePart));
        if (typeIt == typeNames.end()) {
            AM_LOG_WARN("Unknown component type in selector: {}", std::string(typePart));
            return out;
        }
        out.selector.type = typeIt->second;
    }

    bool sawPseudo = false;
    std::string_view pseudoName;

    while (i < sel.size()) {
        char sigil = sel[i];
        size_t segStart = i + 1;
        size_t segEnd = sel.find_first_of(".#:", segStart);
        std::string_view seg = sel.substr(segStart, (segEnd == std::string_view::npos ? sel.size() : segEnd) - segStart);
        i = segEnd == std::string_view::npos ? sel.size() : segEnd;

        if (seg.empty()) {
            AM_LOG_WARN("Malformed selector segment in: {}", std::string(sel));
            return out;
        }
        if (sawPseudo) {
            AM_LOG_WARN("A pseudo-state must end the selector: {}", std::string(sel));
            return out;
        }

        if (sigil == '.') {
            if (out.selector.cls.has_value()) {
                AM_LOG_WARN("Selector names more than one class: {}", std::string(sel));
                return out;
            }
            if (out.selector.part != ComponentPart::NONE) {
                AM_LOG_WARN("A class must come before the #part: {}", std::string(sel));
                return out;
            }
            out.className = std::string(seg);
            out.selector.cls = Style::classToken(out.className);
        } else if (sigil == '#') {
            if (out.selector.part != ComponentPart::NONE) {
                AM_LOG_WARN("Selector names more than one part: {}", std::string(sel));
                return out;
            }
            const auto &partNames = Style::getPartNames();
            auto partIt = partNames.find(std::string(seg));
            if (partIt == partNames.end()) {
                AM_LOG_WARN("Unknown part name in selector: {}", std::string(seg));
                return out;
            }
            out.selector.part = partIt->second;
        } else { // ':'
            sawPseudo = true;
            pseudoName = seg;
        }
    }

    if (sawPseudo && !s_parsePseudoState(pseudoName, out.selector.state)) {
        AM_LOG_WARN("Unknown pseudo-state ':{}'; skipping rule", std::string(pseudoName));
        return out;
    }

    if (!out.selector.type.has_value() && !out.selector.cls.has_value() && out.selector.part == ComponentPart::NONE) {
        AM_LOG_WARN("Selector names no type, class or part: {}", std::string(sel));
        return out;
    }

    out.valid = true;
    return out;
}

static void s_applySelector(const SelectorSpec &sel, const std::vector<Decl> &decls, Style &style, uint32_t &order)
{
    if (!sel.valid) {
        return;
    }
    if (sel.selector.cls.has_value()) {
        style.registerClassName(*sel.selector.cls, sel.className);
    }
    uint32_t o = order++;
    for (const auto &[prop, value] : decls) {
        style.addRule(sel.selector, o, prop, value);
    }
}

/**
 * @brief Strip block and line comments, leaving quoted strings and parenthesised values (which may
 * hold a "//" of their own, e.g. a URL) untouched.
 */
static std::string s_stripComments(std::string_view src)
{
    std::string out;
    out.reserve(src.size());
    char quote = 0;
    int parenDepth = 0;
    for (size_t i = 0; i < src.size();) {
        char c = src[i];
        if (quote != 0) {
            out.push_back(c);
            if (c == quote) {
                quote = 0;
            } else if (c == '\n') {
                quote = 0; // an unterminated string ends with its line rather than eating the file
            }
            ++i;
            continue;
        }
        if (i + 1 < src.size() && c == '/' && src[i + 1] == '*') {
            size_t end = src.find("*/", i + 2);
            if (end == std::string_view::npos) {
                AM_LOG_WARN("Unterminated block comment");
                break;
            }
            i = end + 2;
            out.push_back(' ');
            continue;
        }
        if (parenDepth == 0 && i + 1 < src.size() && c == '/' && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '(') {
            ++parenDepth;
        } else if (c == ')' && parenDepth > 0) {
            --parenDepth;
        } else if (c == '\n' || c == ';' || c == '{' || c == '}') {
            parenDepth = 0; // a stray '(' never outlives its declaration
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

/**
 * @brief Skip a brace-delimited block, nested braces included.
 * @param bracePos Position of the block's opening '{'
 * @return Position just after its matching '}', or the end of the source
 */
static size_t s_skipBlock(std::string_view src, size_t bracePos)
{
    int depth = 0;
    for (size_t i = bracePos; i < src.size(); ++i) {
        if (src[i] == '{') {
            ++depth;
        } else if (src[i] == '}' && --depth == 0) {
            return i + 1;
        }
    }
    return src.size();
}

/**
 * @brief Skip an at-rule this parser does not know: up to its ';', or past its {...} block if the
 * block comes first, so the rule after it is not swallowed into the at-rule's prelude.
 * @return Position just after the skipped at-rule
 */
static size_t s_skipUnknownAtRule(std::string_view src, size_t pos)
{
    size_t semi = src.find(';', pos);
    size_t brace = src.find('{', pos);
    if (semi != std::string_view::npos && (brace == std::string_view::npos || semi < brace)) {
        return semi + 1;
    }
    if (brace == std::string_view::npos) {
        return src.size();
    }
    return s_skipBlock(src, brace);
}

static Style s_parseSource(std::string_view rawSource)
{
    std::string source = s_stripComments(rawSource);
    std::string_view src = source;

    Style style;
    VarMap vars;
    uint32_t order = 0;
    size_t pos = 0;

    while (true) {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            ++pos;
        }
        if (pos >= src.size()) {
            break;
        }

        if (src.substr(pos, 9) == "@property" &&
            (pos + 9 >= src.size() || std::isspace(static_cast<unsigned char>(src[pos + 9])))) {
            size_t eol = src.find('\n', pos);
            std::string_view directive =
                s_trim(src.substr(pos, eol == std::string_view::npos ? std::string_view::npos : eol - pos));
            std::string_view rest = s_trim(directive.substr(9));
            size_t sep = 0;
            while (sep < rest.size() && !std::isspace(static_cast<unsigned char>(rest[sep]))) {
                ++sep;
            }
            std::string_view name = rest.substr(0, sep);
            std::string_view literal = s_trim(rest.substr(sep));
            if (!literal.empty() && literal.back() == ';') {
                literal = s_trim(literal.substr(0, literal.size() - 1));
            }
            if (name.empty() || literal.empty()) {
                AM_LOG_WARN("Malformed @property directive: {}", std::string(directive));
            } else {
                std::optional<std::string> value = s_substituteVars(literal, vars);
                if (value.has_value()) {
                    vars[std::string(name)] = std::string(s_trim(*value));
                }
            }
            pos = eol == std::string_view::npos ? src.size() : eol + 1;
            continue;
        }

        if (src[pos] == '@') {
            size_t nameEnd = pos + 1;
            while (nameEnd < src.size() && s_isVariableNameChar(src[nameEnd])) {
                ++nameEnd;
            }
            AM_LOG_WARN("Unsupported at-rule '{}'; skipping it", std::string(src.substr(pos, nameEnd - pos)));
            pos = s_skipUnknownAtRule(src, pos);
            continue;
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
        // a '{' before the closing brace means a nested block, which the syntax has no use for
        size_t nested = src.find('{', braceOpen + 1);
        if (nested != std::string_view::npos && nested < braceClose) {
            AM_LOG_WARN("Nested '{{' in rule body; skipping rule");
            pos = s_skipBlock(src, braceOpen);
            continue;
        }

        std::string_view selectorList = src.substr(pos, braceOpen - pos);
        std::string_view block = src.substr(braceOpen + 1, braceClose - braceOpen - 1);
        pos = braceClose + 1;

        std::vector<Decl> decls = s_parseBlock(block, style, vars);
        for (std::string_view selText : s_split(selectorList, ',')) {
            if (selText.empty()) {
                AM_LOG_WARN("Empty entry in selector list: {}", std::string(s_trim(selectorList)));
                continue;
            }
            SelectorSpec sel = s_parseSelector(selText);
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
