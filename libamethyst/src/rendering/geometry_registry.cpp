#include "geometry_registry.h"

#include "utils/am_assert.h"

namespace Amethyst {

GeometryAllocation *GeometryRegistry::submit(const InstanceData &data)
{
    uint32_t index = static_cast<uint32_t>(m_allocations.size());
    m_allocations.push_back(data);

    auto alloc = std::make_unique<GeometryAllocation>(index);
    GeometryAllocation *allocPtr = alloc.get();
    m_handleMap.push_back(std::move(alloc));
    m_dirtyIndices.insert(index);
    return allocPtr;
}

void GeometryRegistry::update(GeometryAllocation &alloc, const InstanceData &data)
{
    AM_ASSERT(alloc.isValid(), "Trying to update an Invalid allocation");
    AM_ASSERT(alloc.index < static_cast<uint32_t>(m_allocations.size()), "Allocation index out of bounds");

    m_allocations[alloc.index] = data;
    m_dirtyIndices.insert(alloc.index);
}

void GeometryRegistry::release(GeometryAllocation &&alloc)
{
    if (!alloc.isValid()) return;

    uint32_t indexToRemove = alloc.index;
    uint32_t lastIndex = static_cast<uint32_t>(m_allocations.size() - 1);

    if (indexToRemove != lastIndex) {
        m_allocations[indexToRemove] = m_allocations[lastIndex];
        m_handleMap[lastIndex]->index = indexToRemove;
        m_handleMap[indexToRemove] = std::move(m_handleMap[lastIndex]);
        m_dirtyIndices.insert(indexToRemove);
    }

    m_allocations.pop_back();
    m_handleMap.pop_back();
}

std::set<uint32_t> GeometryRegistry::consumeDirtyIndices()
{
    return std::exchange(m_dirtyIndices, {});
}

} // namespace Amethyst
