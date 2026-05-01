#include "geometry_registry.h"

#include "components/ui_layer.h"
#include "logging/log.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace Amethyst {

std::vector<GeometryRegistry *> GeometryRegistry::s_registries;
GeometryRegistryDestroyCb GeometryRegistry::s_onDestroyCb;

GeometryRegistry::GeometryRegistry(UILayer *owner) : m_owningLayer(owner)
{
    if (!m_owningLayer) {
        return;
    }
    auto it = std::lower_bound(s_registries.begin(), s_registries.end(), this, [](GeometryRegistry *a, GeometryRegistry *b) {
        return a->m_owningLayer->getDisplayOrder() < b->m_owningLayer->getDisplayOrder();
    });
    s_registries.insert(it, this);
}

GeometryRegistry::~GeometryRegistry()
{
    if (s_onDestroyCb) {
        s_onDestroyCb(this);
    }

    auto it = std::find(s_registries.begin(), s_registries.end(), this);
    if (it != s_registries.end()) {
        s_registries.erase(it);
    }
}

std::unique_ptr<GeometryRegistry> GeometryRegistry::create(UILayer *owner)
{
    return std::unique_ptr<GeometryRegistry>(new GeometryRegistry(owner));
}

const std::vector<GeometryRegistry *> &GeometryRegistry::getRegistries()
{
    return s_registries;
}

void GeometryRegistry::resortRegistries()
{
    if (s_registries.size() <= 1) {
        return;
    }
    std::sort(s_registries.begin(), s_registries.end(), [](GeometryRegistry *a, GeometryRegistry *b) {
        return a->m_owningLayer->getDisplayOrder() < b->m_owningLayer->getDisplayOrder();
    });
}

void GeometryRegistry::setDestroyCb(GeometryRegistryDestroyCb cb)
{
    s_onDestroyCb = std::move(cb);
}

GeometryAllocation *GeometryRegistry::submit(const InstanceData &data)
{
    AM_PROFILE_FUNCTION();
    int32_t bucketIndex = data.zIndex;
    auto &currBucket = m_zIndexBuckets[bucketIndex];
    const uint32_t endPos = static_cast<uint32_t>(m_allocations.size());

    m_allocations.push_back(data);
    auto newAlloc = std::make_unique<GeometryAllocation>(endPos, this);
    auto *allocPtr = newAlloc.get();
    m_handleMap.push_back(std::move(newAlloc));

    uint32_t insertPos;
    if (currBucket.count == 0) {
        auto it = m_zIndexBuckets.upper_bound(bucketIndex);
        if (it != m_zIndexBuckets.end() && it->second.count > 0) {
            insertPos = it->second.start;
        } else {
            insertPos = endPos;
        }
        currBucket.start = insertPos;
    } else {
        insertPos = currBucket.start + currBucket.count;
    }

    currBucket.count++;
    m_dirtyIndices.insert(insertPos);

    for (uint32_t pos = endPos; pos > insertPos; --pos) {
        std::swap(m_allocations[pos], m_allocations[pos - 1]);
        std::swap(m_handleMap[pos], m_handleMap[pos - 1]);
        m_handleMap[pos]->index = pos;
        m_handleMap[pos - 1]->index = pos - 1;
        m_dirtyIndices.insert(pos);
        m_dirtyIndices.insert(pos - 1);
    }

    rebuildZIndexBuckets();
    // validateOrdering();
    return allocPtr;
}

void GeometryRegistry::update(GeometryAllocation &alloc, const InstanceData &data)
{
    AM_PROFILE_FUNCTION();
    AM_ASSERT(alloc.isValid(), "Trying to update an Invalid allocation");
    AM_ASSERT(alloc.index < static_cast<uint32_t>(m_allocations.size()), "Allocation index out of bounds");

    m_allocations[alloc.index] = data;
    m_dirtyIndices.insert(alloc.index);

    // validateOrdering();
}

void GeometryRegistry::release(GeometryAllocation &alloc)
{
    AM_PROFILE_FUNCTION();
    if (!alloc.isValid()) return;

    AM_ASSERT(alloc.registry == this, "Allocation belongs to a different registry");
    AM_ASSERT(alloc.index < static_cast<uint32_t>(m_allocations.size()), "Allocation index out of bounds");

    uint32_t indexToRemove = alloc.index;
    uint32_t oldSize = static_cast<uint32_t>(m_allocations.size());

    m_allocations.erase(m_allocations.begin() + indexToRemove);
    m_handleMap.erase(m_handleMap.begin() + indexToRemove);

    for (uint32_t i = indexToRemove; i < m_handleMap.size(); ++i) {
        m_handleMap[i]->index = i;
        m_dirtyIndices.insert(i);
    }

    auto it = m_dirtyIndices.lower_bound(m_allocations.size());
    while (it != m_dirtyIndices.end()) {
        it = m_dirtyIndices.erase(it);
    }

    rebuildZIndexBuckets();
    // validateOrdering();
}

void GeometryRegistry::rebuildZIndexBuckets()
{
    m_zIndexBuckets.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_allocations.size()); ++i) {
        int32_t zIndex = m_allocations[i].zIndex;
        auto &bucket = m_zIndexBuckets[zIndex];
        if (bucket.count == 0) {
            bucket.start = i;
        }
        ++bucket.count;
    }
}

void GeometryRegistry::validateOrdering() const
{
    for (size_t i = 1; i < m_allocations.size(); ++i) {
        AM_ASSERT(m_allocations[i].zIndex >= m_allocations[i - 1].zIndex, "Z-index ordering violation");
        AM_ASSERT(m_handleMap[i]->index == i, "handle index out of sync");
    }
}

std::set<uint32_t> GeometryRegistry::consumeDirtyIndices()
{
    return std::exchange(m_dirtyIndices, {});
}

} // namespace Amethyst
