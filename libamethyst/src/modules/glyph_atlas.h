/**
 * @file glyph_atlas.h
 * @brief GPU-backed glyph atlas with skyline packing for text rendering
 */

#ifndef AMETHYST__GLYPH_ATLAS_H
#define AMETHYST__GLYPH_ATLAS_H

#include "components/common.h"
#include "parsers/freetype/font_loader.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Amethyst {

/**
 * @brief Cached glyph information including atlas region and metrics
 */
struct GlyphInfo {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float advance = 0.0f;
};

/**
 * @brief Glyph atlas manager with on-demand rasterization and texture packing
 *
 * Manages a CPU-side grayscale atlas (1024x1024) that packs glyphs on demand
 * using skyline bottom-left packing. Caches glyphs by (codepoint, pixelSize) key.
 * Backends can sync the atlas to GPU when isDirty() returns true.
 */
class GlyphAtlas {
  public:
    /**
     * @brief Construct glyph atlas with specified font loader
     * @param fontLoader Font loader for rasterizing glyphs (non-owning pointer)
     */
    explicit GlyphAtlas(FontLoader *fontLoader);
    ~GlyphAtlas();

    GlyphAtlas(const GlyphAtlas &) = delete;
    GlyphAtlas &operator=(const GlyphAtlas &) = delete;
    GlyphAtlas(GlyphAtlas &&) noexcept;
    GlyphAtlas &operator=(GlyphAtlas &&) noexcept;

    /**
     * @brief Get cached glyph info, rasterizing on cache miss
     * @param codepoint Unicode codepoint
     * @param pixelSize Font size in pixels
     * @return Pointer to cached glyph info, or nullptr if glyph is empty
     */
    const GlyphInfo *getGlyph(uint32_t codepoint, uint32_t pixelSize);

    /**
     * @brief Get font metrics for specified pixel size
     * @param pixelSize Font size in pixels
     * @return Font metrics (ascender, descender, line height)
     */
    FontMetrics getMetrics(uint32_t pixelSize);

    /**
     * @brief Get kerning adjustment between two glyphs
     * @param left Left glyph codepoint
     * @param right Right glyph codepoint
     * @param pixelSize Font size in pixels
     * @return Horizontal kerning offset in pixels
     */
    float getKerning(uint32_t left, uint32_t right, uint32_t pixelSize);

    /**
     * @brief Get raw atlas pixel data
     * @return Pointer to grayscale uint8_t buffer (single channel)
     */
    const uint8_t *getPixels() const { return m_pixels.data(); }

    /**
     * @brief Get atlas width in pixels
     */
    uint32_t getWidth() const { return m_width; }

    /**
     * @brief Get atlas height in pixels
     */
    uint32_t getHeight() const { return m_height; }

    /**
     * @brief Check if atlas has been modified since last clearDirty()
     * @return true if new glyphs were added
     */
    bool isDirty() const { return m_dirty; }

    /**
     * @brief Clear dirty flag (backend should call after uploading to GPU)
     */
    void clearDirty() { m_dirty = false; }

    /**
     * @brief Set GPU texture ID (backend sets this after creating texture)
     * @param textureId Backend texture handle
     */
    void setTextureId(AmTextureId textureId) { m_textureId = textureId; }

    /**
     * @brief Get GPU texture ID
     * @return Backend texture handle, or AM_INVALID_TEXTURE if not set
     */
    AmTextureId getTextureId() const { return m_textureId; }

  private:
    struct SkylineNode {
        int x;
        int y;
        int width;
    };

    bool packRect(uint32_t width, uint32_t height, uint16_t &outX, uint16_t &outY);
    void insertSkylineNode(int index, const SkylineNode &node);
    void mergeSkylineNodes();

    FontLoader *m_fontLoader;
    std::vector<uint8_t> m_pixels;
    std::vector<SkylineNode> m_skyline;
    std::unordered_map<uint64_t, GlyphInfo> m_cache;
    AmTextureId m_textureId = AM_INVALID_TEXTURE;
    uint32_t m_width;
    uint32_t m_height;
    bool m_dirty = false;
};

} // namespace Amethyst

#endif // AMETHYST__GLYPH_ATLAS_H
