#ifndef AMETHYST_GEOMETRY_REGISTRY_H
#define AMETHYST_GEOMETRY_REGISTRY_H

#include "../components/common.h"

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace Amethyst {

struct GeometryAllocation {
    uint32_t index = UINT32_MAX;

    bool isValid() const { return index != UINT32_MAX; }
};

class GeometryRegistry {
  public:
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

  private:
    std::vector<InstanceData> m_allocations;
    std::vector<std::unique_ptr<GeometryAllocation>> m_handleMap;
    std::set<uint32_t> m_dirtyIndices;
};

} // namespace Amethyst

#endif // AMETHYST_GEOMETRY_REGISTRY_H
