#include "glyph_atlas.h"
#include "logging/log.h"
#include <cstring>

namespace Amethyst {

GlyphAtlas::GlyphAtlas(FontLoader *fontLoader)
    : m_fontLoader(fontLoader), m_width(1024), m_height(1024), m_packer(1024, 1024)
{
    m_pixels.resize(m_width * m_height, 0);
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlas::GlyphAtlas(GlyphAtlas &&) noexcept = default;
GlyphAtlas &GlyphAtlas::operator=(GlyphAtlas &&) noexcept = default;

const GlyphInfo *GlyphAtlas::getGlyph(uint32_t codepoint, uint32_t pixelSize)
{
    uint64_t key = (static_cast<uint64_t>(pixelSize) << 32) | codepoint;

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return &it->second;
    }

    m_fontLoader->setPixelSize(pixelSize);
    GlyphBitmap bitmap = m_fontLoader->rasterizeGlyph(codepoint);

    GlyphInfo info;
    info.bearingX = bitmap.bearingX;
    info.bearingY = bitmap.bearingY;
    info.advance = bitmap.advance;

    if (bitmap.width == 0 || bitmap.height == 0) {
        info.x = 0;
        info.y = 0;
        info.width = 0;
        info.height = 0;
        m_cache[key] = info;
        return &m_cache[key];
    }

    auto region = m_packer.packRect(bitmap.width + 2, bitmap.height + 2);
    if (!region) {
        AM_LOG_ERROR("Glyph atlas full, cannot pack glyph {} at size {}", codepoint, pixelSize);
        return nullptr;
    }

    for (uint32_t row = 0; row < bitmap.height; ++row) {
        uint32_t atlasY = region->y + 1 + row;
        uint32_t atlasX = region->x + 1;
        uint32_t atlasOffset = atlasY * m_width + atlasX;
        uint32_t bitmapOffset = row * bitmap.width;
        std::memcpy(&m_pixels[atlasOffset], &bitmap.buffer[bitmapOffset], bitmap.width);
    }

    info.x = region->x + 1;
    info.y = region->y + 1;
    info.width = static_cast<uint16_t>(bitmap.width);
    info.height = static_cast<uint16_t>(bitmap.height);

    m_cache[key] = info;
    m_dirty = true;

    return &m_cache[key];
}

FontMetrics GlyphAtlas::getMetrics(uint32_t pixelSize)
{
    m_fontLoader->setPixelSize(pixelSize);
    return m_fontLoader->getMetrics();
}

float GlyphAtlas::getKerning(uint32_t left, uint32_t right, uint32_t pixelSize)
{
    m_fontLoader->setPixelSize(pixelSize);
    return m_fontLoader->getKerning(left, right);
}

} // namespace Amethyst
