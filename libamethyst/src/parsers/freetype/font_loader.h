/**
 * @file font_loader.h
 * @brief FreeType-based font loading and glyph rasterization
 */

#ifndef AMETHYST__FONT_LOADER_H
#define AMETHYST__FONT_LOADER_H

#include "math/math.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Amethyst {

struct GlyphBitmap {
    std::vector<uint8_t> buffer;
    uint32_t width = 0;
    uint32_t height = 0;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float advance = 0.0f;
};

struct FontMetrics {
    float ascender = 0.0f;
    float descender = 0.0f;
    float lineHeight = 0.0f;
};

/**
 * @brief The family and style a font file declares for itself.
 */
struct FontDescription {
    std::string family;
    std::string style;
    bool bold = false;
    bool italic = false;
};

class FontLoader {
  public:
    FontLoader();
    ~FontLoader();

    FontLoader(const FontLoader &) = delete;
    FontLoader &operator=(const FontLoader &) = delete;
    FontLoader(FontLoader &&) noexcept;
    FontLoader &operator=(FontLoader &&) noexcept;

    bool loadFont(const std::string &path);
    void setPixelSize(uint32_t size);

    GlyphBitmap rasterizeGlyph(uint32_t codepoint);

    /**
     * @brief Advance width for a codepoint without rendering or loading its outline.
     * @param codepoint Unicode codepoint; unmapped codepoints report the .notdef advance
     * @return Advance in pixels at the current pixel size
     */
    float getAdvance(uint32_t codepoint) const;

    /**
     * @brief The family and style the loaded file names itself.
     * @return The declared description, empty if no font is loaded
     */
    FontDescription getDescription() const;

    FontMetrics getMetrics() const;
    uint32_t getGlyphIndex(uint32_t codepoint) const;
    float getKerning(uint32_t leftCodepoint, uint32_t rightCodepoint) const;

    bool isLoaded() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Amethyst

#endif // AMETHYST__FONT_LOADER_H
