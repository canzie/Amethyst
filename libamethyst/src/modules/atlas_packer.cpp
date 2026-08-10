#include "atlas_packer.h"
#include <algorithm>

namespace Amethyst {

SkylinePacker::SkylinePacker(uint32_t width, uint32_t height) : m_width(width), m_height(height)
{
    m_skyline.push_back({0, 0, static_cast<int32_t>(m_width)});
}

std::optional<AtlasRegion> SkylinePacker::packRect(uint32_t width, uint32_t height)
{
    int32_t bestIndex = -1;
    int32_t bestY = INT32_MAX;
    int32_t bestWidth = INT32_MAX;
    uint16_t outX = 0;
    uint16_t outY = 0;

    for (size_t i = 0; i < m_skyline.size(); ++i) {
        int32_t y = m_skyline[i].y;
        int32_t x = m_skyline[i].x;
        int32_t widthLeft = static_cast<int32_t>(width);

        if (x + static_cast<int32_t>(width) > static_cast<int32_t>(m_width)) {
            continue;
        }

        size_t j = i;
        while (widthLeft > 0 && j < m_skyline.size()) {
            y = std::max(y, m_skyline[j].y);
            if (y + static_cast<int32_t>(height) > static_cast<int32_t>(m_height)) {
                break;
            }
            widthLeft -= m_skyline[j].width;
            ++j;
        }

        if (widthLeft <= 0) {
            if (y < bestY || (y == bestY && m_skyline[i].width < bestWidth)) {
                bestIndex = static_cast<int32_t>(i);
                bestY = y;
                bestWidth = m_skyline[i].width;
                outX = static_cast<uint16_t>(m_skyline[i].x);
                outY = static_cast<uint16_t>(y);
            }
        }
    }

    if (bestIndex == -1) {
        return std::nullopt;
    }

    SkylineNode newNode;
    newNode.x = static_cast<int32_t>(outX);
    newNode.y = static_cast<int32_t>(outY) + static_cast<int32_t>(height);
    newNode.width = static_cast<int32_t>(width);

    insertSkylineNode(bestIndex, newNode);
    mergeSkylineNodes();

    return AtlasRegion{outX, outY, static_cast<uint16_t>(width), static_cast<uint16_t>(height)};
}

void SkylinePacker::insertSkylineNode(int32_t index, const SkylineNode &node)
{
    m_skyline.insert(m_skyline.begin() + index, node);

    for (size_t i = index + 1; i < m_skyline.size(); ++i) {
        if (m_skyline[i].x < m_skyline[i - 1].x + m_skyline[i - 1].width) {
            int32_t shrink = m_skyline[i - 1].x + m_skyline[i - 1].width - m_skyline[i].x;
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

void SkylinePacker::mergeSkylineNodes()
{
    for (size_t i = 0; i + 1 < m_skyline.size(); ++i) {
        if (m_skyline[i].y == m_skyline[i + 1].y) {
            m_skyline[i].width += m_skyline[i + 1].width;
            m_skyline.erase(m_skyline.begin() + i + 1);
            --i;
        }
    }
}

void SkylinePacker::reset()
{
    m_skyline.clear();
    m_skyline.push_back({0, 0, static_cast<int32_t>(m_width)});
}

uint32_t SkylinePacker::getWidth() const
{
    return m_width;
}

uint32_t SkylinePacker::getHeight() const
{
    return m_height;
}

uint64_t SkylinePacker::footprint() const
{
    uint64_t total = 0;
    for (const SkylineNode &node : m_skyline) {
        total += static_cast<uint64_t>(node.y) * static_cast<uint64_t>(node.width);
    }
    return total;
}

ShelfPacker::ShelfPacker(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

std::optional<AtlasRegion> ShelfPacker::packRect(uint32_t width, uint32_t height)
{
    if (width > m_width) {
        return std::nullopt;
    }

    if (m_x + width > m_width) {
        m_rowY += m_rowHeight;
        m_x = 0;
        m_rowHeight = 0;
    }

    if (m_rowY + height > m_height) {
        return std::nullopt;
    }

    AtlasRegion region{static_cast<uint16_t>(m_x), static_cast<uint16_t>(m_rowY), static_cast<uint16_t>(width),
                       static_cast<uint16_t>(height)};

    m_x += width;
    m_rowHeight = std::max(m_rowHeight, height);

    return region;
}

void ShelfPacker::reset()
{
    m_x = 0;
    m_rowY = 0;
    m_rowHeight = 0;
}

uint32_t ShelfPacker::getWidth() const
{
    return m_width;
}

uint32_t ShelfPacker::getHeight() const
{
    return m_height;
}

uint64_t ShelfPacker::footprint() const
{
    return static_cast<uint64_t>(m_rowY + m_rowHeight) * static_cast<uint64_t>(m_width);
}

} // namespace Amethyst
