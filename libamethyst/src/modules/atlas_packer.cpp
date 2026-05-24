#include "atlas_packer.h"
#include <algorithm>

namespace Amethyst {

AtlasPacker::AtlasPacker(uint32_t width, uint32_t height) : m_width(width), m_height(height)
{
    m_skyline.push_back({0, 0, static_cast<int32_t>(m_width)});
}

std::optional<AtlasRegion> AtlasPacker::packRect(uint32_t width, uint32_t height)
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

void AtlasPacker::insertSkylineNode(int32_t index, const SkylineNode &node)
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

void AtlasPacker::mergeSkylineNodes()
{
    for (size_t i = 0; i + 1 < m_skyline.size(); ++i) {
        if (m_skyline[i].y == m_skyline[i + 1].y) {
            m_skyline[i].width += m_skyline[i + 1].width;
            m_skyline.erase(m_skyline.begin() + i + 1);
            --i;
        }
    }
}

void AtlasPacker::reset()
{
    m_skyline.clear();
    m_skyline.push_back({0, 0, static_cast<int32_t>(m_width)});
}

uint32_t AtlasPacker::getWidth() const
{
    return m_width;
}

uint32_t AtlasPacker::getHeight() const
{
    return m_height;
}

} // namespace Amethyst
