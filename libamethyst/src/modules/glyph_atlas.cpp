#include "glyph_atlas.h"
#include "logging/log.h"
#include "utils/am_assert.h"
#include <cstring>

namespace Amethyst {

GlyphAtlas::AtlasPage::AtlasPage(uint32_t width, uint32_t height) : pixels(width * height, 0), packer(width, height) {}

GlyphAtlas::GlyphAtlas() = default;

void GlyphAtlas::init(PageTextureFactory factory)
{
    AM_ASSERT(static_cast<bool>(factory), "glyph atlas needs a texture source");
    m_pageTextureFactory = std::move(factory);
    addPage();
}

uint16_t GlyphAtlas::addPage()
{
    m_pages.emplace_back(PAGE_SIZE, PAGE_SIZE);
    m_pages.back().textureId = m_pageTextureFactory();
    return static_cast<uint16_t>(m_pages.size() - 1);
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlas::GlyphAtlas(GlyphAtlas &&) noexcept = default;
GlyphAtlas &GlyphAtlas::operator=(GlyphAtlas &&) noexcept = default;

GlyphAtlas::SizeGlyphTable &GlyphAtlas::getSizeTable(FontId font, uint32_t pixelSize)
{
    uint32_t key = tableKey(FontRegistry::instance().resolveFont(font), pixelSize);
    if (key == m_lastKey && m_lastTable != nullptr) {
        return *m_lastTable;
    }
    SizeGlyphTable &table = m_sizeTables[key];
    table.font = FontId{static_cast<uint16_t>(key >> 16)};
    table.pixelSize = pixelSize;
    m_lastKey = key;
    m_lastTable = &table;
    return table;
}

bool GlyphAtlas::placeGlyph(uint16_t page, const GlyphBitmap &bitmap, GlyphInfo &out)
{
    AM_ASSERT(!m_pages.empty(), "glyph requested before the atlas was given its texture source");
    out.page = page;

    if (bitmap.width == 0 || bitmap.height == 0) {
        out.x = 0;
        out.y = 0;
        out.width = 0;
        out.height = 0;
        return true;
    }

    auto region = m_pages[page].packer.packRect(bitmap.width + 2, bitmap.height + 2);
    if (!region) {
        return false;
    }

    AtlasPage &target = m_pages[page];
    for (uint32_t row = 0; row < bitmap.height; ++row) {
        uint32_t atlasY = region->y + 1 + row;
        uint32_t atlasX = region->x + 1;
        uint32_t atlasOffset = atlasY * PAGE_SIZE + atlasX;
        uint32_t bitmapOffset = row * bitmap.width;
        std::memcpy(&target.pixels[atlasOffset], &bitmap.buffer[bitmapOffset], bitmap.width);
    }

    out.x = region->x + 1;
    out.y = region->y + 1;
    out.width = static_cast<uint16_t>(bitmap.width);
    out.height = static_cast<uint16_t>(bitmap.height);
    target.dirty = true;
    return true;
}

bool GlyphAtlas::rasterizeGlyphInfo(uint32_t codepoint, SizeGlyphTable &table, GlyphInfo &out)
{
    FontLoader *loader = FontRegistry::instance().getLoader(table.font);
    if (loader == nullptr) {
        return false;
    }

    loader->setPixelSize(table.pixelSize);
    GlyphBitmap bitmap = loader->rasterizeGlyph(codepoint);

    out.bearingX = bitmap.bearingX;
    out.bearingY = bitmap.bearingY;
    out.advance = bitmap.advance;

    if (placeGlyph(table.page, bitmap, out)) {
        return true;
    }

    // The group's page follows the spill, so later glyphs of a run stay together.
    for (uint16_t page = static_cast<uint16_t>(table.page + 1); page < m_pages.size(); page++) {
        if (placeGlyph(page, bitmap, out)) {
            table.page = page;
            return true;
        }
    }

    if (m_pages.size() < MAX_PAGES) {
        uint16_t page = addPage();
        if (placeGlyph(page, bitmap, out)) {
            table.page = page;
            return true;
        }
    }

    AM_LOG_ERROR("Glyph atlas full at {} pages, cannot pack glyph {} at size {}", m_pages.size(), codepoint, table.pixelSize);
    return false;
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

const GlyphInfo *GlyphAtlas::getGlyph(FontId font, uint32_t codepoint, uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(font, pixelSize);
    Entry entry = obtainEntry(table, codepoint);

    if (entry.existed && entry.info->packed) {
        return entry.info;
    }

    GlyphInfo info;
    if (!rasterizeGlyphInfo(codepoint, table, info)) {
        // Packing failed, but the advance is still worth keeping so measurement stays right.
        if (!entry.existed) {
            if (FontLoader *loader = FontRegistry::instance().getLoader(table.font)) {
                loader->setPixelSize(pixelSize);
                entry.info->advance = loader->getAdvance(codepoint);
            }
        }
        return nullptr;
    }
    info.packed = true;

    *entry.info = info;
    return entry.info;
}

float GlyphAtlas::getAdvance(FontId font, uint32_t codepoint, uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(font, pixelSize);
    Entry entry = obtainEntry(table, codepoint);

    if (!entry.existed) {
        if (FontLoader *loader = FontRegistry::instance().getLoader(table.font)) {
            loader->setPixelSize(pixelSize);
            entry.info->advance = loader->getAdvance(codepoint);
        }
    }

    return entry.info->advance;
}

FontMetrics GlyphAtlas::getMetrics(FontId font, uint32_t pixelSize)
{
    SizeGlyphTable &table = getSizeTable(font, pixelSize);
    if (!table.metricsLoaded) {
        FontLoader *loader = FontRegistry::instance().getLoader(table.font);
        if (loader == nullptr) {
            return table.metrics;
        }
        loader->setPixelSize(pixelSize);
        table.metrics = loader->getMetrics();
        table.metricsLoaded = true;
    }
    return table.metrics;
}

float GlyphAtlas::getKerning(FontId font, uint32_t left, uint32_t right, uint32_t pixelSize)
{
    FontLoader *loader = FontRegistry::instance().getLoader(font);
    if (loader == nullptr) {
        return 0.0f;
    }
    loader->setPixelSize(pixelSize);
    return loader->getKerning(left, right);
}

const uint8_t *GlyphAtlas::getPixels(uint16_t page) const
{
    if (page >= m_pages.size()) {
        return nullptr;
    }
    return m_pages[page].pixels.data();
}

bool GlyphAtlas::isDirty(uint16_t page) const
{
    if (page >= m_pages.size()) {
        return false;
    }
    return m_pages[page].dirty;
}

void GlyphAtlas::clearDirty(uint16_t page)
{
    if (page < m_pages.size()) {
        m_pages[page].dirty = false;
    }
}

AmTextureId GlyphAtlas::getTextureId(uint16_t page) const
{
    if (page >= m_pages.size()) {
        return AM_INVALID_TEXTURE;
    }
    return m_pages[page].textureId;
}

} // namespace Amethyst
