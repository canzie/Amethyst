#include "rendering/gpu_arena.h"

#include "amethyst/amethyst_backend.h"
#include "logging/log.h"

#include <algorithm>
#include <cstddef>

namespace Amethyst {

void BlockAllocator::init(uint32_t capacity)
{
    m_capacity = capacity;
    m_free = {{0, capacity}};
}

uint32_t BlockAllocator::alloc(uint32_t n)
{
    if (n == 0) {
        return INVALID;
    }

    for (size_t i = 0; i < m_free.size(); ++i) {
        if (m_free[i].length >= n) {
            uint32_t offset = m_free[i].offset;
            m_free[i].offset += n;
            m_free[i].length -= n;
            if (m_free[i].length == 0) {
                m_free.erase(m_free.begin() + i);
            }
            return offset;
        }
    }
    return INVALID;
}

void BlockAllocator::release(uint32_t offset, uint32_t length)
{
    if (length == 0) {
        return;
    }

    size_t pos = 0;
    while (pos < m_free.size() && m_free[pos].offset < offset) {
        ++pos;
    }

    bool mergedPrev = false;
    if (pos > 0 && m_free[pos - 1].offset + m_free[pos - 1].length == offset) {
        m_free[pos - 1].length += length;
        mergedPrev = true;
    }

    if (mergedPrev) {
        if (pos < m_free.size() && offset + length == m_free[pos].offset) {
            m_free[pos - 1].length += m_free[pos].length;
            m_free.erase(m_free.begin() + pos);
        }
    } else {
        if (pos < m_free.size() && offset + length == m_free[pos].offset) {
            m_free[pos].offset = offset;
            m_free[pos].length += length;
        } else {
            m_free.insert(m_free.begin() + pos, {offset, length});
        }
    }
}

bool BlockAllocator::growInPlace(uint32_t offset, uint32_t oldLength, uint32_t newLength)
{
    if (newLength <= oldLength) {
        return true;
    }

    uint32_t extra = newLength - oldLength;
    uint32_t tail = offset + oldLength;

    for (size_t i = 0; i < m_free.size(); ++i) {
        if (m_free[i].offset == tail) {
            if (m_free[i].length < extra) {
                return false;
            }
            m_free[i].offset += extra;
            m_free[i].length -= extra;
            if (m_free[i].length == 0) {
                m_free.erase(m_free.begin() + i);
            }
            return true;
        }
    }
    return false;
}

void BlockAllocator::rebuildFreelist(uint32_t usedEnd)
{
    m_free.clear();
    if (usedEnd < m_capacity) {
        m_free.push_back({usedEnd, m_capacity - usedEnd});
    }
}

void BlockAllocator::grow(uint32_t newCapacity)
{
    if (newCapacity <= m_capacity) {
        return;
    }

    uint32_t added = newCapacity - m_capacity;
    if (!m_free.empty() && m_free.back().offset + m_free.back().length == m_capacity) {
        m_free.back().length += added;
    } else {
        m_free.push_back({m_capacity, added});
    }
    m_capacity = newCapacity;
}

void GpuArena::init(AmethystBackend &backend, const AmBufferDesc &desc, GrowthPolicy policy)
{
    m_backend = &backend;
    m_policy = policy;
    m_size = 0;
    m_capacity = desc.initialCapacity;
    m_freeList.clear();
    m_retired.clear();
    m_frame = 0;
    m_id = backend.createBuffer(desc);
}

ArenaBlock GpuArena::alloc(size_t size)
{
    if (!m_id.isValid() || size == 0) {
        return {};
    }

    for (auto it = m_freeList.begin(); it != m_freeList.end(); ++it) {
        if (it->size >= size) {
            ArenaBlock block;
            block.offset = it->offset;
            block.capacity = size;

            if (it->size > size * 2) {
                it->offset += size;
                it->size -= size;
            } else {
                block.capacity = it->size;
                m_freeList.erase(it);
            }
            return block;
        }
    }

    if (size > m_capacity - m_size) {
        if (!grow(m_size + size)) {
            AM_LOG_ERROR("GpuArena out of memory and growth failed: size={}, capacity={}, requested={}", m_size, m_capacity, size);
            return {};
        }
    }

    ArenaBlock block;
    block.offset = m_size;
    block.capacity = size;
    m_size += size;
    return block;
}

void GpuArena::free(const ArenaBlock &block)
{
    if (!block.isValid()) {
        return;
    }
    if (m_retireLatency == 0) {
        release(block);
        return;
    }
    m_retired.push_back({block, m_frame + m_retireLatency});
}

void GpuArena::advanceFrame()
{
    ++m_frame;

    size_t kept = 0;
    for (size_t i = 0; i < m_retired.size(); ++i) {
        if (m_retired[i].releaseFrame <= m_frame) {
            release(m_retired[i].block);
        } else {
            m_retired[kept++] = m_retired[i];
        }
    }
    m_retired.resize(kept);
}

size_t GpuArena::retiredBytes() const
{
    size_t total = 0;
    for (const RetiredBlock &retired : m_retired) {
        total += retired.block.capacity;
    }
    return total;
}

size_t GpuArena::freeBytes() const
{
    size_t total = 0;
    for (const FreeBlock &free : m_freeList) {
        total += free.size;
    }
    return total;
}

void GpuArena::release(const ArenaBlock &block)
{
    // keep the list sorted so a block freed between two free neighbours merges with both
    auto next = m_freeList.begin();
    while (next != m_freeList.end() && next->offset < block.offset) {
        ++next;
    }

    size_t pos = static_cast<size_t>(next - m_freeList.begin());
    bool mergedPrev = false;
    if (pos > 0 && m_freeList[pos - 1].offset + m_freeList[pos - 1].size == block.offset) {
        m_freeList[pos - 1].size += block.capacity;
        mergedPrev = true;
    }

    bool touchesNext = pos < m_freeList.size() && block.offset + block.capacity == m_freeList[pos].offset;
    if (mergedPrev && touchesNext) {
        m_freeList[pos - 1].size += m_freeList[pos].size;
        m_freeList.erase(m_freeList.begin() + static_cast<std::ptrdiff_t>(pos));
    } else if (touchesNext) {
        m_freeList[pos].offset = block.offset;
        m_freeList[pos].size += block.capacity;
    } else if (!mergedPrev) {
        m_freeList.insert(m_freeList.begin() + static_cast<std::ptrdiff_t>(pos), {block.offset, block.capacity});
    }
}

void GpuArena::upload(void *cmdBuffer, const void *data, size_t offsetBytes, size_t sizeBytes)
{
    if (!m_id.isValid() || sizeBytes == 0) {
        return;
    }
    m_backend->uploadBufferRange(cmdBuffer, m_id, data, offsetBytes, sizeBytes);
}

bool GpuArena::grow(size_t neededCapacity)
{
    if (m_policy.maxBytes == 0 || m_capacity >= m_policy.maxBytes) {
        return false;
    }

    size_t newCapacity = std::max<size_t>(m_capacity, 1) * 2;
    while (newCapacity < neededCapacity && newCapacity < m_policy.maxBytes) {
        newCapacity *= 2;
    }
    if (newCapacity > m_policy.maxBytes) {
        newCapacity = m_policy.maxBytes;
    }
    if (newCapacity < neededCapacity) {
        return false;
    }

    if (!m_backend->growBuffer(m_id, newCapacity)) {
        return false;
    }

    m_capacity = newCapacity;
    return true;
}

} // namespace Amethyst
