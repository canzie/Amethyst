#include "geometry_registry.h"

#include "utils/am_assert.h"

namespace Amethyst {

uint32_t GeometryRegistry::submit(const InstanceData &data)
{
    uint32_t index;

    if (!m_freeList.empty()) {
        index = m_freeList.back();
        m_freeList.pop_back();
        m_allocations[index] = data;
    } else {
        index = static_cast<uint32_t>(m_allocations.size());
        m_allocations.push_back(data);
    }

    m_dirtyIndices.insert(index);
    return index;
}

void GeometryRegistry::update(uint32_t index, const InstanceData &data)
{
    AM_ASSERT(index < static_cast<uint32_t>(m_allocations.size()), "Not a valid allocation");
    m_allocations[index] = data;
    m_dirtyIndices.insert(index);
}

void GeometryRegistry::release(uint32_t index)
{
    m_freeList.push_back(index);
}

std::set<uint32_t> GeometryRegistry::consumeDirtyIndices()
{
    return std::move(m_dirtyIndices);
}

} // namespace Amethyst
