/**
 * @file glyph_atlas.h
 * @brief Paged GPU-backed glyph atlas for text rendering
 */

#ifndef AMETHYST__GLYPH_ATLAS_H
#define AMETHYST__GLYPH_ATLAS_H

#include "atlas_packer.h"
#include "components/common.h"
#include "parsers/freetype/font_loader.h"
#include "parsers/freetype/font_registry.h"
#include <cstdint>
#include <functional>
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
    uint16_t page = 0;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float advance = 0.0f;
    bool packed = false; // false while only the advance is known; see GlyphAtlas::getAdvance
};

/**
 * @brief Glyph atlas manager with on-demand rasterization and texture packing
 *
 * Manages a list of CPU-side grayscale pages that pack glyphs on demand, caching them by
 * (face, pixel size, codepoint). Pages are added as earlier ones fill, up to MAX_PAGES.
 * Backends can sync a page to GPU when isDirty() reports it.
 *
 * Every glyph of one (face, size) lives on that group's current page, so a run of text at
 * one face and size always resolves to a single texture.
 */
class GlyphAtlas {
  public:
    static constexpr uint32_t PAGE_SIZE = 1024;
    static constexpr uint16_t MAX_PAGES = 64;

    /**
     * @brief Supplies the texture backing a page, at the moment the page is created.
     */
    using PageTextureFactory = std::function<AmTextureId()>;

    GlyphAtlas();
    ~GlyphAtlas();

    /**
     * @brief Give the atlas its texture source and open its first page.
     *
     * A page can be created while laying text out, which is too late to wait for the next
     * upload: the id is read straight into the instance being built. So a page takes its
     * texture the moment it is added, and no glyph can be packed before this is called.
     *
     * @param factory Creates one page-sized texture per call
     */
    void init(PageTextureFactory factory);

    GlyphAtlas(const GlyphAtlas &) = delete;
    GlyphAtlas &operator=(const GlyphAtlas &) = delete;
    GlyphAtlas(GlyphAtlas &&) noexcept;
    GlyphAtlas &operator=(GlyphAtlas &&) noexcept;

    /**
     * @brief Get cached glyph info, rasterizing on cache miss
     * @param font Face to take the glyph from; an invalid id uses the registry default
     * @param codepoint Unicode codepoint
     * @param pixelSize Font size in pixels
     * @return Pointer to cached glyph info, or nullptr if glyph is empty
     */
    const GlyphInfo *getGlyph(FontId font, uint32_t codepoint, uint32_t pixelSize);

    /**
     * @brief Advance width only, without rasterizing or packing the glyph.
     *
     * Measurement must not pull glyphs into the atlas: pages are a fixed size and never
     * evict, so measuring text that is never drawn would consume them permanently. Entries
     * created here are upgraded in place if the glyph is later drawn.
     *
     * @param font Face to measure against; an invalid id uses the registry default
     * @param codepoint Unicode codepoint
     * @param pixelSize Font size in pixels
     * @return Advance width in pixels
     */
    float getAdvance(FontId font, uint32_t codepoint, uint32_t pixelSize);

    /**
     * @brief Get font metrics for specified pixel size
     * @param font Face to query; an invalid id uses the registry default
     * @param pixelSize Font size in pixels
     * @return Font metrics (ascender, descender, line height)
     */
    FontMetrics getMetrics(FontId font, uint32_t pixelSize);

    /**
     * @brief Get kerning adjustment between two glyphs
     * @param font Face to query; an invalid id uses the registry default
     * @param left Left glyph codepoint
     * @param right Right glyph codepoint
     * @param pixelSize Font size in pixels
     * @return Horizontal kerning offset in pixels
     */
    float getKerning(FontId font, uint32_t left, uint32_t right, uint32_t pixelSize);

    /**
     * @brief Number of pages currently allocated.
     */
    uint16_t pageCount() const { return static_cast<uint16_t>(m_pages.size()); }

    /**
     * @brief Get raw pixel data for a page
     * @param page Page to read
     * @return Pointer to grayscale uint8_t buffer (single channel), or nullptr for an unknown page
     */
    const uint8_t *getPixels(uint16_t page) const;

    /**
     * @brief Get page width in pixels
     */
    uint32_t getWidth() const { return PAGE_SIZE; }

    /**
     * @brief Get page height in pixels
     */
    uint32_t getHeight() const { return PAGE_SIZE; }

    /**
     * @brief Check if a page has been modified since its last clearDirty()
     * @param page Page to query
     * @return true if new glyphs were added to it
     */
    bool isDirty(uint16_t page) const;

    /**
     * @brief Clear a page's dirty flag (backend should call after uploading it)
     * @param page Page that was uploaded
     */
    void clearDirty(uint16_t page);

    /**
     * @brief Get a page's GPU texture ID
     * @param page Page to query
     * @return Backend texture handle, or AM_INVALID_TEXTURE if not set
     */
    AmTextureId getTextureId(uint16_t page) const;

  private:
    static constexpr uint32_t ASCII_COUNT = 128;

    /**
     * @brief One packed grayscale page and the texture backing it.
     */
    struct AtlasPage {
        explicit AtlasPage(uint32_t width, uint32_t height);

        std::vector<uint8_t> pixels;
        AtlasPacker packer;
        AmTextureId textureId = AM_INVALID_TEXTURE;
        bool dirty = false;
    };

    /**
     * @brief Per (face, pixel size) glyph cache: flat array for ASCII, map for the rest.
     *
     * ASCII (the common case) resolves to a direct array index instead of a
     * hashmap probe, and metrics are cached to avoid re-querying the font loader.
     */
    struct SizeGlyphTable {
        GlyphInfo ascii[ASCII_COUNT];
        bool asciiLoaded[ASCII_COUNT] = {};
        std::unordered_map<uint32_t, GlyphInfo> extended;
        FontMetrics metrics;
        FontId font;
        uint32_t pixelSize = 0;
        bool metricsLoaded = false;
        uint16_t page = 0;
    };

    SizeGlyphTable &getSizeTable(FontId font, uint32_t pixelSize);
    bool rasterizeGlyphInfo(uint32_t codepoint, SizeGlyphTable &table, GlyphInfo &out);

    /**
     * @brief Copy a rasterized glyph into a specific page, reserving space for it there.
     * @param page Page to pack into
     * @param bitmap Rasterized glyph; an empty bitmap is placed without reserving anything
     * @param out Glyph record to fill with the resulting region and page
     * @return True if the page had room, false if it is full
     */
    bool placeGlyph(uint16_t page, const GlyphBitmap &bitmap, GlyphInfo &out);

    struct Entry {
        GlyphInfo *info = nullptr;
        bool existed = false;
    };

    /**
     * @brief Get the cache entry for a codepoint, creating an empty one if absent.
     * @param table Per-group table to look in
     * @param codepoint Unicode codepoint
     * @return The entry and whether it already held loaded data
     */
    static Entry obtainEntry(SizeGlyphTable &table, uint32_t codepoint);

    /**
     * @brief Cache key combining a face id with a pixel size.
     */
    static uint32_t tableKey(FontId font, uint32_t pixelSize)
    {
        return (static_cast<uint32_t>(font.index) << 16) | (pixelSize & 0xFFFFu);
    }

    /**
     * @brief Append a page, giving it a texture straight away.
     * @return Index of the new page
     */
    uint16_t addPage();

    PageTextureFactory m_pageTextureFactory;
    std::vector<AtlasPage> m_pages;
    std::unordered_map<uint32_t, SizeGlyphTable> m_sizeTables;
    uint32_t m_lastKey = UINT32_MAX;
    SizeGlyphTable *m_lastTable = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__GLYPH_ATLAS_H
