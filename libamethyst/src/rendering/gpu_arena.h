/**
 * @file gpu_arena.h
 * @brief Shared GPU buffer substrate: dirty tracking, block allocation and suballocated backend buffers
 */

#ifndef AMETHYST__GPU_ARENA_H
#define AMETHYST__GPU_ARENA_H

#include "components/common.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Amethyst {

class AmethystBackend;

/**
 * @brief Half-open dirty interval [lo, hi) over a buffer, for incremental GPU upload.
 */
struct DirtyRange {
    uint32_t lo = UINT32_MAX;
    uint32_t hi = 0;

    bool empty() const { return lo >= hi; }

    void add(uint32_t start, uint32_t end)
    {
        if (start < lo) {
            lo = start;
        }
        if (end > hi) {
            hi = end;
        }
    }

    void clear()
    {
        lo = UINT32_MAX;
        hi = 0;
    }
};

/**
 * @brief First-fit block allocator over a fixed-capacity arena with free-span coalescing.
 *
 * Hands out exact-size blocks of slots from a single arena. Growth and slack policy live
 * in the caller; this allocator only tracks free spans.
 */
class BlockAllocator {
  public:
    static constexpr uint32_t INVALID = UINT32_MAX;

    /**
     * @brief Reset the arena to a single free span covering its whole capacity.
     * @param capacity Total number of slots the arena manages.
     */
    void init(uint32_t capacity);

    /**
     * @brief Allocate exactly n slots from the first free span that fits.
     * @param n Number of slots to allocate.
     * @return Block offset into the arena, or INVALID if no free span is large enough.
     */
    uint32_t alloc(uint32_t n);

    /**
     * @brief Return a block to the free list, coalescing with adjacent free spans.
     * @param offset Block offset previously returned by alloc.
     * @param length Block length in slots.
     */
    void release(uint32_t offset, uint32_t length);

    /**
     * @brief Try to extend a block in place using the immediately following free span.
     * @param offset Block offset to extend.
     * @param oldLength Current block length in slots.
     * @param newLength Desired block length in slots.
     * @return True if extended in place (no data moves), false if the caller must relocate.
     */
    bool growInPlace(uint32_t offset, uint32_t oldLength, uint32_t newLength);

    /**
     * @brief Replace the free list with a single tail span [usedEnd, capacity), used after compaction.
     * @param usedEnd First free slot once all live blocks have been packed to the front.
     */
    void rebuildFreelist(uint32_t usedEnd);

    /**
     * @brief Raise the capacity, offering the added slots as free space. Live blocks do not move.
     * @param newCapacity Total number of slots the arena manages; ignored if not larger.
     */
    void grow(uint32_t newCapacity);

    /**
     * @brief Total number of slots the arena manages.
     * @return Arena capacity in slots.
     */
    uint32_t capacity() const { return m_capacity; }

  private:
    struct FreeSpan {
        uint32_t offset;
        uint32_t length;
    };

    std::vector<FreeSpan> m_free;
    uint32_t m_capacity = 0;
};

/**
 * @brief Growth policy for a GpuArena. maxBytes of 0 means the arena never grows.
 */
struct GrowthPolicy {
    size_t maxBytes = 0;

    static GrowthPolicy fixed() { return {0}; }
    static GrowthPolicy doubleUntil(size_t cap) { return {cap}; }
};

/**
 * @brief A byte range suballocated from a GpuArena.
 */
struct ArenaBlock {
    size_t offset = 0;
    size_t capacity = 0;

    bool isValid() const { return capacity > 0; }
};

/**
 * @brief One backend buffer plus the suballocation and growth policy layered on top of it.
 *
 * Owns the AmBufferId and a byte-granular free list. Growth decisions happen here; the
 * backend only executes growBuffer and rebinds the descriptor recorded in the desc. The
 * arena holds no CPU mirror: producers own the canonical data and pass it to upload().
 */
class GpuArena {
  public:
    /**
     * @brief Create the backend buffer and adopt its growth policy.
     * @param backend Backend the buffer lives on.
     * @param desc Buffer description, including initial capacity and shader binding.
     * @param policy Growth policy applied when the free list and tail are exhausted.
     */
    void init(AmethystBackend &backend, const AmBufferDesc &desc, GrowthPolicy policy);

    /**
     * @brief Suballocate a byte range, growing the backend buffer if the policy allows.
     * @param size Number of bytes requested.
     * @return A valid block, or an invalid block if the arena is exhausted.
     */
    ArenaBlock alloc(size_t size);

    /**
     * @brief Return a block to the free list, coalescing with adjacent free blocks.
     * @param block Block previously returned by alloc.
     */
    void free(const ArenaBlock &block);

    /**
     * @brief Upload bytes into the arena's backend buffer.
     * @param cmdBuffer Backend-native command buffer, forwarded to uploadBufferRange.
     * @param data Source bytes.
     * @param offsetBytes Destination offset within the arena.
     * @param sizeBytes Number of bytes to upload.
     */
    void upload(void *cmdBuffer, const void *data, size_t offsetBytes, size_t sizeBytes);

    AmBufferId bufferId() const { return m_id; }

  private:
    struct FreeBlock {
        size_t offset;
        size_t size;
    };

    bool grow(size_t neededCapacity);

    AmethystBackend *m_backend = nullptr;
    AmBufferId m_id;
    GrowthPolicy m_policy;
    size_t m_size = 0;
    size_t m_capacity = 0;
    std::vector<FreeBlock> m_freeList;
};

} // namespace Amethyst

#endif // AMETHYST__GPU_ARENA_H
