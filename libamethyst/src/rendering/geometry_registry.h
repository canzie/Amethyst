#ifndef AMETHYST_GEOMETRY_REGISTRY_H
#define AMETHYST_GEOMETRY_REGISTRY_H

#include "../components/common.h"

#include <cstdint>
#include <set>
#include <vector>

namespace Amethyst {

class GeometryRegistry {
  public:
    /**
     * @brief Submit new instance data.
     * @return Index/handle to the allocation.
     */
    uint32_t submit(const InstanceData &data);

    /**
     * @brief Update existing instance data.
     */
    void update(uint32_t index, const InstanceData &data);

    /**
     * @brief Release an allocation.
     */
    void release(uint32_t index);

    /**
     * @brief Get dirty indices and clear the list.
     *
     * Backend calls this to know which instances need re-upload.
     */
    std::set<uint32_t> consumeDirtyIndices();

    const std::vector<InstanceData> &getAllocations() const { return m_allocations; }
    size_t size() const { return m_allocations.size(); }

  private:
    std::vector<InstanceData> m_allocations;
    std::set<uint32_t> m_dirtyIndices;
    std::vector<uint32_t> m_freeList;
};

} // namespace Amethyst

#endif // AMETHYST_GEOMETRY_REGISTRY_H
