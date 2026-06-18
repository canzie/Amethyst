/**
 * @file font_locator.h
 * @brief Discovery of installed system fonts by family name, across Windows/macOS/Linux
 */

#ifndef AMETHYST__FONT_LOCATOR_H
#define AMETHYST__FONT_LOCATOR_H

#include <optional>
#include <string>

namespace Amethyst {

struct FontMatch {
    std::string familyName;
    std::string path;
};

/**
 * @brief Search installed system fonts for the closest match to a family name.
 * @param familyName Font family name to search for, e.g. "Arial" or "Noto Sans"
 * @return The closest installed font found, or std::nullopt if nothing close enough was found
 */
std::optional<FontMatch> findClosestFont(const std::string &familyName);

} // namespace Amethyst

#endif // AMETHYST__FONT_LOCATOR_H
