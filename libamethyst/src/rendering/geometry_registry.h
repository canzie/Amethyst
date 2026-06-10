#ifndef AMETHYST__GEOMETRY_REGISTRY_H
#define AMETHYST__GEOMETRY_REGISTRY_H

#include "modules/glyph_buffer.h"
#include "rendering/instance_data.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace Amethyst {

class GeometryRegistry;
class UILayer;

using GeometryRegistryDestroyCb = std::function<void(GeometryRegistry *)>;

struct GeometryAllocation {
    uint32_t slotId = UINT32_MAX;
    GeometryRegistry *registry = nullptr;
    bool owning = true; // used for children, if a component could possibly not own the allocation they use, this will be false,
                        // true in case they litterly called submit themselves or if the component explicitly knows it owns it, and
                        // can just ignore the flag

    bool isValid() const { return slotId != UINT32_MAX && registry != nullptr; }
};

class GeometryRegistry {
  public:
    /**
     * @brief Create a new GeometryRegistry owned by a UILayer
     * @param owner The owning UILayer
     * @return Unique pointer to the created registry
     */
    static std::unique_ptr<GeometryRegistry> create(UILayer *owner);

    /**
     * @brief Get all registries sorted by owner displayOrder
     * @return Reference to the sorted registries vector
     */
    static const std::vector<GeometryRegistry *> &getRegistries();

    /**
     * @brief Re-sort registries by owner displayOrder
     */
    static void resortRegistries();

    /**
     * @brief Set callback invoked when a registry is destroyed
     * @param cb Callback receiving the destroyed registry pointer
     */
    static void setDestroyCb(GeometryRegistryDestroyCb cb);

    /**
     * @brief Submit new instance data
     * @param data The instance data to store
     * @return Stable handle pointer, valid until release() is called on it
     */
    GeometryAllocation *submit(const InstanceData &data);

    /**
     * @brief Update existing instance data in-place
     * @param alloc The allocation handle to update
     * @param data The new instance data
     */
    void update(GeometryAllocation &alloc, const InstanceData &data);

    /**
     * @brief Get mutable access to an allocation's instance data, marking it dirty for re-upload
     *
     * For cheap in-place edits that do not change sort order, e.g. shifting translation,
     * toggling visibility or updating the clip rect when a cached element only moved.
     *
     * @warning Do not change zIndex through this pointer; the sort order is not rebuilt. Use update() for that.
     * @param alloc The allocation handle to mutate
     * @return Pointer to the slot's instance data, or nullptr if the allocation is invalid
     */
    InstanceData *getMutable(const GeometryAllocation &alloc);

    /**
     * @brief Release an allocation, freeing its slot for reuse
     * @param alloc The allocation handle to release
     */
    void release(GeometryAllocation &alloc);

    /**
     * @brief Rebuild the sorted buffer from slot data. Must be called before getAllocations().
     */
    void flush();

    /**
     * @brief Get the z-sorted instance data buffer
     * @return Reference to the sorted buffer
     */
    const std::vector<InstanceData> &getAllocations() const { return m_sortedBuffer; }
    size_t size() const { return m_sortedBuffer.size(); }

    /**
     * @brief True if the sorted buffer was fully rebuilt since last clearDirtyState()
     * @return Whether a full GPU re-upload is needed
     */
    bool isFullDirty() const { return m_fullDirty; }

    /**
     * @brief Get GPU buffer positions that changed incrementally since last clearDirtyState()
     * @return Reference to the dirty indices vector
     */
    const std::vector<uint32_t> &getDirtyIndices() const { return m_dirtyGpuIndices; }

    /**
     * @brief Reset dirty tracking after GPU upload is complete
     */
    void clearDirtyState();

    /**
     * @brief Get the owning UILayer
     * @return Pointer to the owning layer
     */
    UILayer *getOwningLayer() const { return m_owningLayer; }

    /**
     * @brief Get the batched-text glyph buffer, creating it on first use.
     * @return Reference to this registry's glyph buffer.
     */
    GlyphBuffer &glyphBuffer();

    /**
     * @brief Get the glyph buffer without creating it.
     * @return Pointer to the glyph buffer, or nullptr if no text slice has been allocated.
     */
    GlyphBuffer *getGlyphBuffer() const { return m_glyphBuffer.get(); }

    ~GeometryRegistry();

  private:
    GeometryRegistry(UILayer *owner);
    void validateOrdering() const;

  private:
    static std::vector<GeometryRegistry *> s_registries;
    static GeometryRegistryDestroyCb s_onDestroyCb;

    UILayer *m_owningLayer = nullptr;

    std::vector<InstanceData> m_slotData;
    std::vector<uint8_t> m_slotAlive;
    std::vector<uint8_t> m_slotDirty;
    std::vector<uint32_t> m_slotFreeList;
    std::vector<uint32_t> m_dirtySlotList;

    std::vector<InstanceData> m_sortedBuffer;
    std::vector<uint32_t> m_sortedOrder;
    std::vector<uint32_t> m_slotToSorted;

    std::deque<GeometryAllocation> m_handlePool;

    bool m_needsRebuild = false;
    bool m_fullDirty = false;
    std::vector<uint32_t> m_dirtyGpuIndices;

    std::unique_ptr<GlyphBuffer> m_glyphBuffer;
};

} // namespace Amethyst

#endif // AMETHYST__GEOMETRY_REGISTRY_H
