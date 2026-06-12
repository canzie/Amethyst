#include "geometry_registry.h"

#include "components/ui_layer.h"
#include "rendering/gpu_resource_hub.h"
#include "utils/am_assert.h"
#include "utils/profiling.h"
#include <algorithm>
#include <cstdint>

namespace Amethyst {

std::vector<GeometryRegistry *> GeometryRegistry::s_registries;

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
    if (GpuResourceHub::active() != nullptr) {
        GpuResourceHub::active()->onRegistryDestroyed(this);
    }

    auto it = std::find(s_registries.begin(), s_registries.end(), this);
    if (it != s_registries.end()) {
        s_registries.erase(it);
    }
}

GlyphBuffer &GeometryRegistry::glyphBuffer()
{
    if (m_glyphBuffer == nullptr) {
        m_glyphBuffer = std::make_unique<GlyphBuffer>();
    }
    return *m_glyphBuffer;
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

GeometryAllocation *GeometryRegistry::submit(const InstanceData &data)
{
    AM_PROFILE_FUNCTION();

    uint32_t slotId;
    if (!m_slotFreeList.empty()) {
        slotId = m_slotFreeList.back();
        m_slotFreeList.pop_back();
    } else {
        slotId = static_cast<uint32_t>(m_slotData.size());
        m_slotData.emplace_back();
        m_slotAlive.push_back(0);
        m_slotDirty.push_back(0);
        m_slotToSorted.push_back(UINT32_MAX);
        m_dirtySlotList.reserve(m_slotData.capacity());
    }

    m_slotData[slotId] = data;
    m_slotAlive[slotId] = 1;
    m_needsRebuild = true;

    m_handlePool.push_back({slotId, this, true});
    return &m_handlePool.back();
}

void GeometryRegistry::update(GeometryAllocation &alloc, const InstanceData &data)
{
    AM_PROFILE_FUNCTION();
    AM_ASSERT(alloc.isValid(), "Trying to update an invalid allocation");
    AM_ASSERT(alloc.slotId < static_cast<uint32_t>(m_slotData.size()), "Slot ID out of bounds");

    if (m_slotData[alloc.slotId].zIndex != data.zIndex) {
        m_needsRebuild = true;
    }

    m_slotData[alloc.slotId] = data;

    if (!m_slotDirty[alloc.slotId]) {
        m_slotDirty[alloc.slotId] = 1;
        m_dirtySlotList.push_back(alloc.slotId);
    }
}

InstanceData *GeometryRegistry::getMutable(const GeometryAllocation &alloc)
{
    if (!alloc.isValid() || alloc.slotId >= static_cast<uint32_t>(m_slotData.size())) {
        return nullptr;
    }

    if (!m_slotDirty[alloc.slotId]) {
        m_slotDirty[alloc.slotId] = 1;
        m_dirtySlotList.push_back(alloc.slotId);
    }
    return &m_slotData[alloc.slotId];
}

void GeometryRegistry::release(GeometryAllocation &alloc)
{
    AM_PROFILE_FUNCTION();
    if (!alloc.isValid()) return;

    AM_ASSERT(alloc.registry == this, "Allocation belongs to a different registry");
    AM_ASSERT(alloc.slotId < static_cast<uint32_t>(m_slotData.size()), "Slot ID out of bounds");

    m_slotAlive[alloc.slotId] = 0;
    m_slotDirty[alloc.slotId] = 0;
    m_slotFreeList.push_back(alloc.slotId);
    m_needsRebuild = true;

    alloc.slotId = UINT32_MAX;
    alloc.registry = nullptr;
}

void GeometryRegistry::flush()
{
    AM_PROFILE_FUNCTION();

    if (m_needsRebuild) {
        m_sortedOrder.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_slotData.size()); ++i) {
            if (m_slotAlive[i]) {
                m_sortedOrder.push_back(i);
            }
        }

        std::stable_sort(m_sortedOrder.begin(), m_sortedOrder.end(),
                         [this](uint32_t a, uint32_t b) { return m_slotData[a].zIndex < m_slotData[b].zIndex; });

        m_sortedBuffer.resize(m_sortedOrder.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_sortedOrder.size()); ++i) {
            uint32_t slot = m_sortedOrder[i];
            m_sortedBuffer[i] = m_slotData[slot];
            m_slotToSorted[slot] = i;
        }

        m_fullDirty = true;
        m_needsRebuild = false;
        m_dirtyGpuIndices.clear();

        for (uint32_t slot : m_dirtySlotList) {
            m_slotDirty[slot] = 0;
        }
        m_dirtySlotList.clear();
        return;
    }

    if (!m_dirtySlotList.empty()) {
        for (uint32_t slot : m_dirtySlotList) {
            if (m_slotAlive[slot]) {
                uint32_t sortedIdx = m_slotToSorted[slot];
                AM_ASSERT(sortedIdx < static_cast<uint32_t>(m_sortedBuffer.size()), "Sorted index out of bounds");
                m_sortedBuffer[sortedIdx] = m_slotData[slot];
                m_dirtyGpuIndices.push_back(sortedIdx);
            }
            m_slotDirty[slot] = 0;
        }
        m_dirtySlotList.clear();
    }
}

void GeometryRegistry::clearDirtyState()
{
    m_fullDirty = false;
    m_dirtyGpuIndices.clear();
}

void GeometryRegistry::validateOrdering() const
{
    for (size_t i = 1; i < m_sortedBuffer.size(); ++i) {
        AM_ASSERT(m_sortedBuffer[i].zIndex >= m_sortedBuffer[i - 1].zIndex, "Z-index ordering violation");
    }
}

} // namespace Amethyst
