/**
 * @file text_registry.h
 * @brief Registry for text rendering data
 */

#ifndef AMETHYST_TEXT_REGISTRY_H
#define AMETHYST_TEXT_REGISTRY_H

#include "components/common.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

class TextRegistry;
class UILayer;

using TextRegistryDestroyCb = std::function<void(TextRegistry *)>;

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
     * @brief Create a new TextRegistry owned by a UILayer
     * @param owner The owning UILayer
     * @return Unique pointer to the created registry
     */
    static std::unique_ptr<TextRegistry> create(UILayer *owner);

    /**
     * @brief Get all registries sorted by owner displayOrder
     */
    static const std::vector<TextRegistry *> &getRegistries();

    /**
     * @brief Re-sort registries by owner displayOrder
     */
    static void resortRegistries();

    /**
     * @brief Set callback invoked when a registry is destroyed
     */
    static void setDestroyCb(TextRegistryDestroyCb cb);

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

    /**
     * @brief Get the owning UILayer
     */
    UILayer *getOwningLayer() const { return m_owningLayer; }

    ~TextRegistry();

  private:
    TextRegistry(UILayer *owner);
    void rebuildFlatBuffer();

  private:
    static std::vector<TextRegistry *> s_registries;
    static TextRegistryDestroyCb s_onDestroyCb;

    UILayer *m_owningLayer = nullptr;
    std::vector<std::vector<CharacterInstance>> m_allocations;
    std::vector<std::unique_ptr<TextAllocation>> m_handleMap;
    std::vector<CharacterInstance> m_flatBuffer;
    size_t m_flatBufferRequiredSize = 0;
    bool m_dirty = false;
};

} // namespace Amethyst

#endif // AMETHYST_TEXT_REGISTRY_H
