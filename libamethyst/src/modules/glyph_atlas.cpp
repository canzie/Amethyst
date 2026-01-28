#include "glyph_atlas.h"
#include "logging/log.h"
#include <algorithm>
#include <cstring>

namespace Amethyst {

GlyphAtlas::GlyphAtlas(FontLoader *fontLoader) : m_fontLoader(fontLoader), m_width(1024), m_height(1024)
{
    m_pixels.resize(m_width * m_height, 0);
    m_skyline.push_back({0, 0, static_cast<int>(m_width)});
}

GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlas::GlyphAtlas(GlyphAtlas &&) noexcept = default;
GlyphAtlas &GlyphAtlas::operator=(GlyphAtlas &&) noexcept = default;

bool GlyphAtlas::packRect(uint32_t width, uint32_t height, uint16_t &outX, uint16_t &outY)
{
    int bestIndex = -1;
    int bestY = INT32_MAX;
    int bestWidth = INT32_MAX;

    for (size_t i = 0; i < m_skyline.size(); ++i) {
        int y = m_skyline[i].y;
        int x = m_skyline[i].x;
        int widthLeft = static_cast<int>(width);

        if (x + static_cast<int>(width) > static_cast<int>(m_width)) {
            continue;
        }

        size_t j = i;
        while (widthLeft > 0 && j < m_skyline.size()) {
            y = std::max(y, m_skyline[j].y);
            if (y + static_cast<int>(height) > static_cast<int>(m_height)) {
                break;
            }
            widthLeft -= m_skyline[j].width;
            ++j;
        }

        if (widthLeft <= 0) {
            if (y < bestY || (y == bestY && m_skyline[i].width < bestWidth)) {
                bestIndex = static_cast<int>(i);
                bestY = y;
                bestWidth = m_skyline[i].width;
                outX = static_cast<uint16_t>(m_skyline[i].x);
                outY = static_cast<uint16_t>(y);
            }
        }
    }

    if (bestIndex == -1) {
        return false;
    }

    SkylineNode newNode;
    newNode.x = static_cast<int>(outX);
    newNode.y = static_cast<int>(outY) + static_cast<int>(height);
    newNode.width = static_cast<int>(width);

    insertSkylineNode(bestIndex, newNode);
    mergeSkylineNodes();

    return true;
}

void GlyphAtlas::insertSkylineNode(int index, const SkylineNode &node)
{
    m_skyline.insert(m_skyline.begin() + index, node);

    for (size_t i = index + 1; i < m_skyline.size(); ++i) {
        if (m_skyline[i].x < m_skyline[i - 1].x + m_skyline[i - 1].width) {
            int shrink = m_skyline[i - 1].x + m_skyline[i - 1].width - m_skyline[i].x;
            m_skyline[i].x += shrink;
            m_skyline[i].width -= shrink;
            if (m_skyline[i].width <= 0) {
                m_skyline.erase(m_skyline.begin() + i);
                --i;
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

void GlyphAtlas::mergeSkylineNodes()
{
    for (size_t i = 0; i + 1 < m_skyline.size(); ++i) {
        if (m_skyline[i].y == m_skyline[i + 1].y) {
            m_skyline[i].width += m_skyline[i + 1].width;
            m_skyline.erase(m_skyline.begin() + i + 1);
            --i;
        }
    }
}

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

    uint16_t x, y;
    if (!packRect(bitmap.width + 2, bitmap.height + 2, x, y)) {
        AM_LOG_ERROR("Glyph atlas full, cannot pack glyph {} at size {}", codepoint, pixelSize);
        return nullptr;
    }

    for (uint32_t row = 0; row < bitmap.height; ++row) {
        uint32_t atlasY = y + 1 + row;
        uint32_t atlasX = x + 1;
        uint32_t atlasOffset = atlasY * m_width + atlasX;
        uint32_t bitmapOffset = row * bitmap.width;
        std::memcpy(&m_pixels[atlasOffset], &bitmap.buffer[bitmapOffset], bitmap.width);
    }

    info.x = x + 1;
    info.y = y + 1;
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
