#ifndef AMETHYST__GEOMETRY_REGISTRY_H
#define AMETHYST__GEOMETRY_REGISTRY_H

#include "components/common.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace Amethyst {

class GeometryRegistry;
class UILayer;

using GeometryRegistryDestroyCb = std::function<void(GeometryRegistry *)>;

struct GeometryAllocation {
    uint32_t index = UINT32_MAX;
    GeometryRegistry *registry = nullptr;

    bool isValid() const { return index != UINT32_MAX && registry != nullptr; }
};

struct ZIndexBucket {
    uint32_t start = 0; // Start offset in m_allocations
    uint32_t count = 0; // Number of elements with this z-index
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
     */
    static const std::vector<GeometryRegistry *> &getRegistries();

    /**
     * @brief Re-sort registries by owner displayOrder
     */
    static void resortRegistries();

    /**
     * @brief Set callback invoked when a registry is destroyed
     */
    static void setDestroyCb(GeometryRegistryDestroyCb cb);

    /**
     * @brief Submit new instance data.
     * @return Allocation whose index may change on release of other allocations.
     */
    GeometryAllocation *submit(const InstanceData &data);

    /**
     * @brief Update existing instance data.
     */
    void update(GeometryAllocation &alloc, const InstanceData &data);

    /**
     * @brief Release an allocation. Swaps with last element to keep buffer compact.
     */
    void release(GeometryAllocation &&alloc);

    /**
     * @brief Get dirty indices and clear the list.
     */
    std::set<uint32_t> consumeDirtyIndices();

    const std::vector<InstanceData> &getAllocations() const { return m_allocations; }
    size_t size() const { return m_allocations.size(); }

    /**
     * @brief Get the owning UILayer
     */
    UILayer *getOwningLayer() const { return m_owningLayer; }

    ~GeometryRegistry();

  private:
    GeometryRegistry(UILayer *owner);
    void validateOrdering() const;

  private:
    static std::vector<GeometryRegistry *> s_registries;
    static GeometryRegistryDestroyCb s_onDestroyCb;

    UILayer *m_owningLayer = nullptr;
    std::vector<InstanceData> m_allocations;
    std::vector<std::unique_ptr<GeometryAllocation>> m_handleMap;
    std::set<uint32_t> m_dirtyIndices;
    std::map<int32_t, ZIndexBucket> m_zIndexBuckets;
};

} // namespace Amethyst

#endif // AMETHYST__GEOMETRY_REGISTRY_H
