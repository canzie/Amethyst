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

    uint32_t currTargetPos;
    if (currBucket.count == 0) {
        auto it = m_zIndexBuckets.upper_bound(bucketIndex);
        if (it != m_zIndexBuckets.end() && it->second.count > 0) {
            currTargetPos = it->second.start;
        } else {
            currTargetPos = endPos;
        }
        currBucket.start = currTargetPos;
    } else {
        currTargetPos = currBucket.start + currBucket.count;
    }

    currBucket.count++;
    m_dirtyIndices.insert(currTargetPos);

    while (endPos > currTargetPos) {
        std::swap(m_allocations[endPos], m_allocations[currTargetPos]);
        std::swap(m_handleMap[endPos], m_handleMap[currTargetPos]);

        // curr is not at the desired pos
        // the thing we just swapped with is now at endPos, so repeat and go;
        m_handleMap[endPos]->index = endPos;
        m_handleMap[currTargetPos]->index = currTargetPos;
        m_dirtyIndices.insert(endPos);
        m_dirtyIndices.insert(currTargetPos);

        auto &swappedBucket = m_zIndexBuckets[m_allocations[endPos].zIndex];
        currTargetPos = swappedBucket.start + swappedBucket.count;
        swappedBucket.start++;
    }

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

    uint32_t indexToRemove = alloc.index;
    const uint32_t endPos = static_cast<uint32_t>(m_allocations.size() - 1);

    int32_t bucketIndex = m_allocations[indexToRemove].zIndex;
    auto &bucket = m_zIndexBuckets[bucketIndex];

    uint32_t bucketEnd = bucket.start + bucket.count - 1;

    if (indexToRemove != bucketEnd) {
        std::swap(m_allocations[indexToRemove], m_allocations[bucketEnd]);
        std::swap(m_handleMap[indexToRemove], m_handleMap[bucketEnd]);
        m_handleMap[indexToRemove]->index = indexToRemove;
        m_handleMap[bucketEnd]->index = bucketEnd;
        m_dirtyIndices.insert(indexToRemove);
    }

    uint32_t currTargetPos = bucketEnd;
    uint32_t nextBucketStart = bucket.start + bucket.count;

    while (endPos > currTargetPos) {
        int32_t nextBucketIndex = m_allocations[nextBucketStart].zIndex;
        auto &nextBucket = m_zIndexBuckets[nextBucketIndex];
        uint32_t nextBucketEnd = nextBucket.start + nextBucket.count - 1;

        // we swap the element we want to get to the back and delete (currTargetPos) with the last element
        // of the next bucket, and make that one the first of its bucket now, after this the currTargetPos is at the end of the
        // nextBucket
        std::swap(m_allocations[nextBucketEnd], m_allocations[currTargetPos]);
        std::swap(m_handleMap[nextBucketEnd], m_handleMap[currTargetPos]);
        nextBucket.start--;

        m_handleMap[currTargetPos]->index = currTargetPos;
        m_dirtyIndices.insert(currTargetPos);

        currTargetPos = nextBucketEnd; // because of the swap
        nextBucketStart = currTargetPos + 1;
    }

    m_dirtyIndices.erase(endPos);
    m_allocations.pop_back();
    m_handleMap.pop_back();
    bucket.count--;

    // validateOrdering();
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
