#include "rendering/gpu_arena.h"

#include "amethyst/amethyst_backend.h"
#include "logging/log.h"

#include <iterator>

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

void GpuArena::init(AmethystBackend &backend, const AmBufferDesc &desc, GrowthPolicy policy)
{
    m_backend = &backend;
    m_policy = policy;
    m_size = 0;
    m_capacity = desc.initialCapacity;
    m_freeList.clear();
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

    if (m_size + size > m_capacity) {
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

    FreeBlock freed;
    freed.offset = block.offset;
    freed.size = block.capacity;

    for (auto it = m_freeList.begin(); it != m_freeList.end(); ++it) {
        if (it->offset + it->size == freed.offset) {
            it->size += freed.size;
            auto next = std::next(it);
            if (next != m_freeList.end() && it->offset + it->size == next->offset) {
                it->size += next->size;
                m_freeList.erase(next);
            }
            return;
        }
        if (freed.offset + freed.size == it->offset) {
            it->offset = freed.offset;
            it->size += freed.size;
            return;
        }
    }

    m_freeList.push_back(freed);
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

    size_t newCapacity = m_capacity * 2;
    while (newCapacity < neededCapacity) {
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
