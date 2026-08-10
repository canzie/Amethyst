#include "glyph_atlas.h"
#include "logging/log.h"
#include <cstring>

namespace Amethyst {

GlyphAtlas::GlyphAtlas(FontLoader *fontLoader) : m_fontLoader(fontLoader), m_width(1024), m_height(1024), m_packer(1024, 1024)
{
    m_pixels.resize(m_width * m_height, 0);
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlas::GlyphAtlas(GlyphAtlas &&) noexcept = default;
GlyphAtlas &GlyphAtlas::operator=(GlyphAtlas &&) noexcept = default;

GlyphAtlas::SizeGlyphTable &GlyphAtlas::getSizeTable(uint32_t pixelSize)
{
    if (pixelSize == m_lastPixelSize && m_lastTable != nullptr) {
        return *m_lastTable;
    }
    SizeGlyphTable &table = m_sizeTables[pixelSize];
    m_lastPixelSize = pixelSize;
    m_lastTable = &table;
    return table;
}

bool GlyphAtlas::rasterizeGlyphInfo(uint32_t codepoint, uint32_t pixelSize, GlyphInfo &out)
{
    m_fontLoader->setPixelSize(pixelSize);
    GlyphBitmap bitmap = m_fontLoader->rasterizeGlyph(codepoint);

    out.bearingX = bitmap.bearingX;
    out.bearingY = bitmap.bearingY;
    out.advance = bitmap.advance;

    if (bitmap.width == 0 || bitmap.height == 0) {
        out.x = 0;
        out.y = 0;
        out.width = 0;
        out.height = 0;
        return true;
    }

    auto region = m_packer.packRect(bitmap.width + 2, bitmap.height + 2);
    if (!region) {
        AM_LOG_ERROR("Glyph atlas full, cannot pack glyph {} at size {}", codepoint, pixelSize);
        return false;
    }

    for (uint32_t row = 0; row < bitmap.height; ++row) {
        uint32_t atlasY = region->y + 1 + row;
        uint32_t atlasX = region->x + 1;
        uint32_t atlasOffset = atlasY * m_width + atlasX;
        uint32_t bitmapOffset = row * bitmap.width;
        std::memcpy(&m_pixels[atlasOffset], &bitmap.buffer[bitmapOffset], bitmap.width);
    }

    out.x = region->x + 1;
    out.y = region->y + 1;
    out.width = static_cast<uint16_t>(bitmap.width);
    out.height = static_cast<uint16_t>(bitmap.height);

    m_dirty = true;
    return true;
}

GlyphAtlas::Entry GlyphAtlas::obtainEntry(SizeGlyphTable &table, uint32_t codepoint)
{
    if (codepoint < ASCII_COUNT) {
        bool existed = table.asciiLoaded[codepoint];
        table.asciiLoaded[codepoint] = true;
        return {&table.ascii[codepoint], existed};
    }

    auto [it, inserted] = table.extended.try_emplace(codepoint);
    return {&it->second, !inserted};
}

const GlyphInfo *GlyphAtlas::getGlyph(uint32_t codepoint, uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(pixelSize);
    Entry entry = obtainEntry(table, codepoint);

    if (entry.existed && entry.info->packed) {
        return entry.info;
    }

    GlyphInfo info;
    if (!rasterizeGlyphInfo(codepoint, pixelSize, info)) {
        // Packing failed, but the advance is still worth keeping so measurement stays right.
        if (!entry.existed) {
            m_fontLoader->setPixelSize(pixelSize);
            entry.info->advance = m_fontLoader->getAdvance(codepoint);
        }
        return nullptr;
    }
    info.packed = true;

    *entry.info = info;
    return entry.info;
}

float GlyphAtlas::getAdvance(uint32_t codepoint, uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(pixelSize);
    Entry entry = obtainEntry(table, codepoint);

    if (!entry.existed) {
        m_fontLoader->setPixelSize(pixelSize);
        entry.info->advance = m_fontLoader->getAdvance(codepoint);
    }

    return entry.info->advance;
}

FontMetrics GlyphAtlas::getMetrics(uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(pixelSize);
    if (!table.metricsLoaded) {
        m_fontLoader->setPixelSize(pixelSize);
        table.metrics = m_fontLoader->getMetrics();
        table.metricsLoaded = true;
    }
    return table.metrics;
}

float GlyphAtlas::getKerning(uint32_t left, uint32_t right, uint32_t pixelSize)
{
    m_fontLoader->setPixelSize(pixelSize);
    return m_fontLoader->getKerning(left, right);
}

} // namespace Amethyst
