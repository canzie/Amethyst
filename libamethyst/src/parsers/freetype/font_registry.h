/**
 * @file font_registry.h
 * @brief Named font faces, addressed by a stable id
 */

#ifndef AMETHYST__FONT_REGISTRY_H
#define AMETHYST__FONT_REGISTRY_H

#include "parsers/freetype/font_loader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

/**
 * @brief Index of a loaded font in the registry.
 */
struct FontId {
    static constexpr uint16_t INVALID = UINT16_MAX;

    uint16_t index = INVALID;

    bool isValid() const { return index != INVALID; }
    bool operator==(const FontId &) const = default;
};

/**
 * @brief Owns every loaded font and resolves a name to an id.
 *
 * Ids are indices and never change, so a font can be looked up once and stored. The first
 * font loaded becomes the default, which is what an unset or unresolvable name falls back to.
 */
class FontRegistry {
  public:
    /**
     * @brief The process-wide set of fonts, which AmethystContext initialises and shuts down.
     * @return Reference to the registry
     */
    static FontRegistry &instance();

    /**
     * @brief Drop every loaded font, releasing the underlying files.
     */
    void shutdown();

    /**
     * @brief Load a font from disk under the family and style the file declares for itself.
     *
     * A font whose style is Regular takes the family alone, so IBMPlexMono-Regular.ttf
     * registers as "IBM Plex Mono" and IBMPlexMono-Bold.ttf as "IBM Plex Mono Bold".
     *
     * @param path Path to the font file
     * @return Id of the font, or an invalid id if the file could not be loaded
     */
    FontId loadFont(const std::string &path);

    /**
     * @brief Load a font from disk under a chosen name, or return the existing id if taken.
     * @param name Name callers and themes refer to the font by, in place of its declared one
     * @param path Path to the font file
     * @return Id of the font, or an invalid id if the file could not be loaded
     */
    FontId loadFont(std::string_view name, const std::string &path);

    /**
     * @brief Look up a font by the name it was loaded under.
     * @param name Name to resolve
     * @return Id of the font, or an invalid id if no font has that name
     */
    FontId findFont(std::string_view name) const;

    /**
     * @brief Font used when a requested id is invalid.
     * @return Id of the default font, or an invalid id if nothing is loaded
     */
    FontId defaultFont() const { return m_default; }

    /**
     * @brief Choose the font that an unset or unresolvable name falls back to.
     * @param id Font to make the default; ignored if invalid
     */
    void setDefaultFont(FontId id);

    /**
     * @brief Loader for a font, falling back to the default for an invalid id.
     * @param id Font to resolve
     * @return Loader for the font, or nullptr if nothing is loaded
     */
    FontLoader *getLoader(FontId id);

    /**
     * @brief Substitute the default for an id that names no loaded font.
     * @param id Font to resolve
     * @return An id naming a loaded font, or an invalid id if nothing is loaded
     */
    FontId resolveFont(FontId id) const;

    uint16_t fontCount() const { return static_cast<uint16_t>(m_fontFaces.size()); }

  private:
    /**
     * @brief Take ownership of a loaded font face under a name, keeping the first if it is taken.
     * @param name Name to register the font face under
     * @param loader Loaded font face
     * @return Id of the font face holding that name
     */
    FontId addFontFace(std::string name, std::unique_ptr<FontLoader> loader);

    struct FontFace {
        std::string name;
        std::unique_ptr<FontLoader> loader;
    };

    std::vector<FontFace> m_fontFaces;
    FontId m_default;
};

} // namespace Amethyst

#endif // AMETHYST__FONT_REGISTRY_H
