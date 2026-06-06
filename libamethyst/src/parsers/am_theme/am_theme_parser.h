#ifndef AMETHYST__AM_THEME_PARSER_H
#define AMETHYST__AM_THEME_PARSER_H

#include "modules/style.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Amethyst {

/**
 * @brief Parses .amstyle (CSS-like) theme files into a Style object.
 *
 * A rule is a comma-separated selector list and a brace-delimited declaration block:
 *   text-button.primary, .danger { background-color: #4772b3; corner-radius: 4px; }
 *
 * Selectors: bare type (text-button), .class (.danger), type.class (table.compact),
 * type#part (collapsible-header#header) for built-in sub-elements. A trailing
 * :pseudo (e.g. :hover) is parsed but currently skipped.
 *
 * Values: lengths require px (corner-radius: 4px), ratios are unitless 0..1
 * (background-transparency: 0.5), dimensions take px/% (padding: 50% + 8px), colors
 * are #rgb / #rrggbb / #rrggbbaa or rgb()/rgba(). Spacing shorthand:
 *   padding: 10px               -> all sides
 *   padding: 10px 20px          -> vertical, horizontal
 *   padding: 10px 20px 30px 40px -> top, right, bottom, left
 */
class AmThemeParser {
  public:
    static std::optional<Style> parseFile(const std::filesystem::path &path);
    static std::optional<Style> parseString(const std::string &source);
};

} // namespace Amethyst

#endif // AMETHYST__AM_THEME_PARSER_H
