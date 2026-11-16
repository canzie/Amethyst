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
 * and for built-in sub-parts of compositing components #part (#header), type#part
 * (tab-bar#tab), .class#part (.primary#tab) and type.class#part (tab-bar.primary#tab).
 * In a part selector the type names the component that owns the part (a TabBar's tab
 * label is a Frame, but `tab-bar#tab` targets it), and the class may sit on the part
 * itself or on its owner: `.abc#tab` styles the tabs of a TabBar carrying `.abc`, while
 * a plain `.abc` rule only styles the TabBar. A trailing :pseudo qualifies any selector
 * by one of the fixed GuiState bits: :hover, :pressed, :disabled, :focused, :active.
 * Specificity is CSS's (parts, classes + pseudos, types); at equal specificity the
 * declaration that comes later in the file wins. Descendant combinators are not
 * supported.
 *
 * Variables: an @property directive on its own line binds a name to a literal
 * (@property accent #4772b3), and @name expands to that literal wherever it
 * appears in a declaration value (background-color: @accent). Bindings are built
 * top to bottom, so a variable must be defined before it is used; referencing an
 * undefined @name logs a warning and drops that declaration.
 *
 * Values: lengths require px (corner-radius: 4px), ratios are unitless 0..1
 * (background-transparency: 0.5), dimensions take px/% (padding: 50% + 8px), colors
 * are #rgb / #rgba / #rrggbb / #rrggbbaa or rgb(r, g, b) / rgba(r, g, b, a) with
 * 0..255 channels and a 0..1 (or percentage) alpha. A malformed value logs a warning
 * and drops its declaration, unknown at-rules are skipped. Spacing shorthand:
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
