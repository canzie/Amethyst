/**
 * @file text_registry.h
 * @brief Registry for text rendering data
 */

#ifndef AMETHYST_TEXT_REGISTRY_H
#define AMETHYST_TEXT_REGISTRY_H

#include "components/common.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

class TextRegistry;

struct TextAllocation {
    uint32_t index = UINT32_MAX;
    TextRegistry *registry = nullptr;

    bool isValid() const { return index != UINT32_MAX && registry != nullptr; }
};

/**
 * @brief Registry for text character instances with allocation tracking
 */
class TextRegistry {
  public:
    /**
     * @brief Submit new text characters
     * @return Allocation handle
     */
    TextAllocation *submit(const std::vector<CharacterInstance> &chars);

    /**
     * @brief Update existing text allocation
     */
    void update(TextAllocation &alloc, const std::vector<CharacterInstance> &chars);

    /**
     * @brief Release an allocation
     */
    void release(TextAllocation &&alloc);

    /**
     * @brief Check if dirty and clear the flag
     */
    bool consumeDirty();

    /**
     * @brief Get flattened character buffer for GPU upload
     */
    const std::vector<CharacterInstance> &getCharacters() const { return m_flatBuffer; }
    size_t size() const { return m_flatBuffer.size(); }
    bool empty() const { return m_flatBuffer.empty(); }

  private:
    void rebuildFlatBuffer();

    std::vector<std::vector<CharacterInstance>> m_allocations;
    std::vector<std::unique_ptr<TextAllocation>> m_handleMap;
    std::vector<CharacterInstance> m_flatBuffer;
    size_t m_flatBufferRequiredSize = 0;
    bool m_dirty = false;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_REGISTRY_H
