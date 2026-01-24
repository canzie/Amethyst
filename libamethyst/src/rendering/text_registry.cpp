/**
 * @file text_registry.cpp
 * @brief TextRegistry implementation
 */

#include "rendering/text_registry.h"

#include "components/ui_layer.h"
#include <algorithm>

namespace Amethyst {

std::vector<TextRegistry *> TextRegistry::s_registries;
TextRegistryDestroyCb TextRegistry::s_onDestroyCb;

TextRegistry::TextRegistry(UILayer *owner) : m_owningLayer(owner)
{
    // Insert into s_registries sorted by owner->displayOrder
    auto it = std::lower_bound(s_registries.begin(), s_registries.end(), this, [](TextRegistry *a, TextRegistry *b) {
        return a->m_owningLayer->getDisplayOrder() < b->m_owningLayer->getDisplayOrder();
    });
    s_registries.insert(it, this);
}

TextRegistry::~TextRegistry()
{
    if (s_onDestroyCb) {
        s_onDestroyCb(this);
    }

    auto it = std::find(s_registries.begin(), s_registries.end(), this);
    if (it != s_registries.end()) {
        s_registries.erase(it);
    }
}

std::unique_ptr<TextRegistry> TextRegistry::create(UILayer *owner)
{
    return std::unique_ptr<TextRegistry>(new TextRegistry(owner));
}

const std::vector<TextRegistry *> &TextRegistry::getRegistries()
{
    return s_registries;
}

void TextRegistry::resortRegistries()
{
    if (s_registries.size() <= 1) {
        return;
    }

    std::sort(s_registries.begin(), s_registries.end(), [](TextRegistry *a, TextRegistry *b) {
        return a->m_owningLayer->getDisplayOrder() < b->m_owningLayer->getDisplayOrder();
    });
}

void TextRegistry::setDestroyCb(TextRegistryDestroyCb cb)
{
    s_onDestroyCb = std::move(cb);
}

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
