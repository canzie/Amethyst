#ifndef AMETHYST__LAYOUT_CONFIG_PARSER_H
#define AMETHYST__LAYOUT_CONFIG_PARSER_H

#include "parsers/config/layout_config.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace Amethyst {

/**
 * @brief (De)serializes the line-based layout config format.
 *
 * One record per line: the first token is the kind (dock/tabbar/branch/leaf/selected),
 * strings are double-quoted, numbers are bare. `dock`/`tabbar` open a named entry that the
 * following lines belong to. Format-only; LayoutConfig owns the data and its lifecycle.
 */
class LayoutConfigParser {
  public:
    static bool read(const std::filesystem::path &path, std::unordered_map<std::string, ConfigEntry> &outEntries);
    static bool write(const std::filesystem::path &path, const std::unordered_map<std::string, ConfigEntry> &entries);
};

} // namespace Amethyst

#endif // AMETHYST__LAYOUT_CONFIG_PARSER_H
