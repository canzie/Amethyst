/**
 * @file text_registry.cpp
 * @brief TextRegistry implementation
 */

#include "rendering/text_registry.h"

namespace Amethyst {

TextAllocation *TextRegistry::submit(const std::vector<CharacterInstance> &chars)
{
    uint32_t index = static_cast<uint32_t>(m_allocations.size());
    m_allocations.push_back(chars);
    m_flatBufferRequiredSize += chars.size();

    auto alloc = std::make_unique<TextAllocation>(index, this);
    TextAllocation *allocPtr = alloc.get();
    m_handleMap.push_back(std::move(alloc));
    m_dirty = true;
    return allocPtr;
}

void TextRegistry::update(TextAllocation &alloc, const std::vector<CharacterInstance> &chars)
{
    if (!alloc.isValid()) return;

    size_t oldSize = m_allocations[alloc.index].size();
    m_flatBufferRequiredSize -= oldSize;
    m_flatBufferRequiredSize += chars.size();
    m_allocations[alloc.index] = chars;
    m_dirty = true;
}

void TextRegistry::release(TextAllocation &&alloc)
{
    if (!alloc.isValid()) return;

    uint32_t indexToRemove = alloc.index;
    uint32_t lastIndex = static_cast<uint32_t>(m_allocations.size() - 1);

    m_flatBufferRequiredSize -= m_allocations[indexToRemove].size();

    if (indexToRemove != lastIndex) {
        m_allocations[indexToRemove] = std::move(m_allocations[lastIndex]);
        m_handleMap[lastIndex]->index = indexToRemove;
        m_handleMap[indexToRemove] = std::move(m_handleMap[lastIndex]);
    }

    m_allocations.pop_back();
    m_handleMap.pop_back();
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
