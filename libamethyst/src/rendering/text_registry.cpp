/**
 * @file text_registry.cpp
 * @brief TextRegistry implementation
 */

#include "rendering/text_registry.h"

namespace Amethyst {

uint32_t TextRegistry::submit(const std::vector<CharacterInstance> &chars)
{
    uint32_t index = static_cast<uint32_t>(m_allocations.size());
    m_allocations.push_back(chars);
    m_flatBufferRequiredSize += chars.size();
    m_dirty = true;
    return index;
}

void TextRegistry::update(uint32_t index, const std::vector<CharacterInstance> &chars)
{
    size_t oldSize = m_allocations[index].size();
    if (oldSize == chars.size()) {
        m_allocations[index] = chars;
    } else {
        m_flatBufferRequiredSize -= oldSize;
        m_flatBufferRequiredSize += chars.size();
        m_allocations[index] = chars;
        m_dirty = true;
    }
}

void TextRegistry::release(uint32_t index)
{
    m_flatBufferRequiredSize -= m_allocations[index].size();
    m_allocations[index].clear();
    m_dirty = true;
}

bool TextRegistry::consumeDirty()
{
    if (!m_dirty) {
        return false;
    }

    rebuildFlatBuffer();
    m_dirty = false;
    return true;
}

void TextRegistry::rebuildFlatBuffer()
{
    m_flatBuffer.clear();
    m_flatBuffer.reserve(m_flatBufferRequiredSize);
    for (const auto &alloc : m_allocations) {
        m_flatBuffer.insert(m_flatBuffer.end(), alloc.begin(), alloc.end());
    }
}

} // namespace Amethyst
