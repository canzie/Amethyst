#ifndef AMETHYST__AM_THEME_PARSER_H
#define AMETHYST__AM_THEME_PARSER_H

#include "modules/style.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Amethyst {

/**
 * @brief Parses .toml theme files into a Style object.
 *
 * Supports shorthand properties:
 *   padding = 10                  -> all sides
 *   padding = [10, 20]            -> vertical, horizontal
 *   padding = [10, 20, 30, 40]    -> top, right, bottom, left
 *   margin follows the same pattern
 *
 * Colors can be specified as:
 *   backgroundColor = "#ff0000"   -> hex
 *   backgroundColor = [255, 0, 0] -> RGB array
 */
class AmThemeParser {
public:
    static std::optional<Style> parseFile(const std::filesystem::path& path);
    static std::optional<Style> parseString(const std::string& tomlContent);
};

} // namespace Amethyst

#endif // AMETHYST__AM_THEME_PARSER_H
