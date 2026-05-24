/**
 * @file svg_atlas.h
 * @brief SVG rasterization and packing into a 2048x2048 RGBA texture atlas
 */

#ifndef AMETHYST__SVG_ATLAS_H
#define AMETHYST__SVG_ATLAS_H

#include "atlas_packer.h"
#include "components/common.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Amethyst {

using SvgHash = size_t;

struct SvgEntry {
    uint16_t atlasX = 0;
    uint16_t atlasY = 0;
    uint16_t width = 0;
    uint16_t height = 0;
};

class SvgAtlas {
  public:
    SvgAtlas();
    ~SvgAtlas();

    SvgAtlas(const SvgAtlas &) = delete;
    SvgAtlas &operator=(const SvgAtlas &) = delete;
    SvgAtlas(SvgAtlas &&) noexcept;
    SvgAtlas &operator=(SvgAtlas &&) noexcept;

    /**
     * @brief Rasterize SVG data at the given size, cached by content hash + dimensions.
     * @param svgData Raw SVG markup.
     * @param width Target rasterization width in pixels.
     * @param height Target rasterization height in pixels.
     * @return Pointer to the packed entry, or nullptr on failure.
     */
    const SvgEntry *loadFromData(const std::string &svgData, uint32_t width, uint32_t height);

    const uint8_t *getPixels() const { return m_pixels.data(); }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    void setTextureId(AmTextureId textureId) { m_textureId = textureId; }
    AmTextureId getTextureId() const { return m_textureId; }

  private:
    const SvgEntry *rasterizeAndPack(SvgHash key, const uint8_t *rgbaData, uint32_t width, uint32_t height, int stride);

    AtlasPacker m_packer;
    std::vector<uint8_t> m_pixels;
    std::unordered_map<SvgHash, SvgEntry> m_cache;
    AmTextureId m_textureId = AM_INVALID_TEXTURE;
    uint32_t m_width;
    uint32_t m_height;
    bool m_dirty = false;
};

} // namespace Amethyst
#endif // AMETHYST__SVG_ATLAS_H
