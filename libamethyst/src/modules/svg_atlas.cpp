#include "svg_atlas.h"
#include "logging/log.h"
#include <cstring>
#include <lunasvg.h>

namespace Amethyst {

SvgAtlas::SvgAtlas() : m_packer(2048, 2048), m_width(2048), m_height(2048)
{
    m_pixels.resize(m_width * m_height * 4, 0);
}

SvgAtlas::~SvgAtlas() = default;

SvgAtlas::SvgAtlas(SvgAtlas &&) noexcept = default;
SvgAtlas &SvgAtlas::operator=(SvgAtlas &&) noexcept = default;

#define HASH_GOLDEN_RATIO 0x9e3779b9
#define HASH_SHIFT_LEFT   6
#define HASH_SHIFT_RIGHT  2
#define HASH_COMBINE(h, val) \
    ((h) ^ (std::hash<uint32_t>{}(val) + HASH_GOLDEN_RATIO + ((h) << HASH_SHIFT_LEFT) + ((h) >> HASH_SHIFT_RIGHT)))

static SvgHash s_computeHash(const std::string &svgData, uint32_t width, uint32_t height)
{
    size_t h = std::hash<std::string>{}(svgData);
    h = HASH_COMBINE(h, width);
    h = HASH_COMBINE(h, height);
    return h;
}

const SvgEntry *SvgAtlas::loadFromData(const std::string &svgData, uint32_t width, uint32_t height)
{
    SvgHash key = s_computeHash(svgData, width, height);

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        return &it->second;
    }

    auto document = lunasvg::Document::loadFromData(svgData);
    if (document == nullptr) {
        AM_LOG_ERROR("SvgAtlas: failed to parse SVG data");
        return nullptr;
    }

    auto bitmap = document->renderToBitmap(static_cast<int>(width), static_cast<int>(height));
    if (bitmap.isNull()) {
        AM_LOG_ERROR("SvgAtlas: failed to rasterize SVG at {}x{}", width, height);
        return nullptr;
    }

    bitmap.convertToRGBA();
    return rasterizeAndPack(key, bitmap.data(), static_cast<uint32_t>(bitmap.width()), static_cast<uint32_t>(bitmap.height()),
                            bitmap.stride());
}

const SvgEntry *SvgAtlas::rasterizeAndPack(SvgHash key, const uint8_t *rgbaData, uint32_t width, uint32_t height, int stride)
{
    // a transparent 1px gutter on every side, like the glyph atlas, so bilinear filtering at an
    // entry's edge never pulls in its neighbour's pixels
    auto region = m_packer.packRect(width + 2 * GUTTER, height + 2 * GUTTER);
    if (!region.has_value()) {
        AM_LOG_ERROR("SvgAtlas: atlas full");
        return nullptr;
    }

    uint32_t originX = region->x + GUTTER;
    uint32_t originY = region->y + GUTTER;
    for (uint32_t row = 0; row < height; ++row) {
        size_t atlasOffset = (static_cast<size_t>(originY + row) * m_width + originX) * 4;
        size_t srcOffset = static_cast<size_t>(row) * static_cast<size_t>(stride);
        std::memcpy(&m_pixels[atlasOffset], &rgbaData[srcOffset], static_cast<size_t>(width) * 4);
    }

    SvgEntry entry;
    entry.atlasX = static_cast<uint16_t>(originX);
    entry.atlasY = static_cast<uint16_t>(originY);
    entry.width = static_cast<uint16_t>(width);
    entry.height = static_cast<uint16_t>(height);

    m_cache[key] = entry;
    m_dirty = true;

    return &m_cache[key];
}

} // namespace Amethyst
