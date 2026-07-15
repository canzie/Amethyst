#include "font_locator.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace Amethyst {

static std::vector<std::filesystem::path> s_systemFontDirectories()
{
    std::vector<std::filesystem::path> dirs;

#ifdef _WIN32
    char windowsDir[MAX_PATH] = {};
    if (GetWindowsDirectoryA(windowsDir, MAX_PATH) != 0) {
        dirs.push_back(std::filesystem::path(windowsDir) / "Fonts");
    }

    const char *localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr) {
        dirs.push_back(std::filesystem::path(localAppData) / "Microsoft" / "Windows" / "Fonts");
    }
#elif defined(__APPLE__)
    dirs.emplace_back("/System/Library/Fonts");
    dirs.emplace_back("/Library/Fonts");

    const char *home = std::getenv("HOME");
    if (home != nullptr) {
        dirs.push_back(std::filesystem::path(home) / "Library" / "Fonts");
    }
#else
    dirs.emplace_back("/usr/share/fonts");
    dirs.emplace_back("/usr/local/share/fonts");

    const char *xdgDataHome = std::getenv("XDG_DATA_HOME");
    const char *home = std::getenv("HOME");
    if (xdgDataHome != nullptr) {
        dirs.push_back(std::filesystem::path(xdgDataHome) / "fonts");
    } else if (home != nullptr) {
        dirs.push_back(std::filesystem::path(home) / ".local" / "share" / "fonts");
    }
    if (home != nullptr) {
        dirs.push_back(std::filesystem::path(home) / ".fonts");
    }
#endif

    return dirs;
}

static bool s_hasFontExtension(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension == ".ttf" || extension == ".otf" || extension == ".ttc" || extension == ".otc";
}

static std::string s_normalizeFontName(const std::string &name)
{
    std::string normalized;
    normalized.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(c)));
        }
    }
    return normalized;
}

static size_t s_levenshteinDistance(const std::string &a, const std::string &b)
{
    std::vector<size_t> previousRow(b.size() + 1);
    std::vector<size_t> currentRow(b.size() + 1);

    for (size_t j = 0; j <= b.size(); ++j) {
        previousRow[j] = j;
    }

    for (size_t i = 1; i <= a.size(); ++i) {
        currentRow[0] = i;
        for (size_t j = 1; j <= b.size(); ++j) {
            size_t substitutionCost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            currentRow[j] = std::min({previousRow[j] + 1, currentRow[j - 1] + 1, previousRow[j - 1] + substitutionCost});
        }
        std::swap(previousRow, currentRow);
    }

    return previousRow[b.size()];
}

static void s_collectFontsFromDirectory(FT_Library library, const std::filesystem::path &directory, std::vector<FontMatch> &fonts)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        return;
    }

    std::filesystem::recursive_directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, error);
    std::filesystem::recursive_directory_iterator end;

    while (it != end && !error) {
        if (it->is_regular_file() && s_hasFontExtension(it->path())) {
            FT_Face face = nullptr;
            if (FT_New_Face(library, it->path().string().c_str(), 0, &face) == 0) {
                if (face->family_name != nullptr) {
                    fonts.push_back({face->family_name, it->path().string()});
                }
                FT_Done_Face(face);
            }
        }

        it.increment(error);
    }
}

static std::vector<FontMatch> s_collectInstalledFonts()
{
    std::vector<FontMatch> fonts;

    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return fonts;
    }

    for (const std::filesystem::path &directory : s_systemFontDirectories()) {
        s_collectFontsFromDirectory(library, directory, fonts);
    }

    FT_Done_FreeType(library);
    return fonts;
}

std::optional<FontMatch> findClosestFont(const std::string &familyName)
{
    std::vector<FontMatch> fonts = s_collectInstalledFonts();
    if (fonts.empty()) {
        return std::nullopt;
    }

    std::string normalizedQuery = s_normalizeFontName(familyName);

    const FontMatch *bestMatch = nullptr;
    size_t bestDistance = SIZE_MAX;

    for (const FontMatch &font : fonts) {
        size_t distance = s_levenshteinDistance(normalizedQuery, s_normalizeFontName(font.familyName));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestMatch = &font;
        }
        if (distance == 0) {
            break;
        }
    }

    size_t threshold = std::max<size_t>(2, normalizedQuery.size() / 2);
    if (bestMatch != nullptr && bestDistance <= threshold) {
        return *bestMatch;
    }

    return std::nullopt;
}

} // namespace Amethyst
