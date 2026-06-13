/**
 * @file gradient_buffer.h
 * @brief Global GPU store of gradient records, interned by definition and reclaimed by refcount
 */

#ifndef AMETHYST__GRADIENT_BUFFER_H
#define AMETHYST__GRADIENT_BUFFER_H

#include "modules/color.h"
#include "rendering/gpu_arena.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Amethyst {

class AmethystBackend;

/**
 * @brief One global gradient SSBO shared across all layers, bound at a fixed descriptor slot.
 *
 * Each distinct Gradient definition is interned to a stable slot on first resolve, and the slot
 * index is what an instance stores. Lifetime rides on the shared_ptr the colors hold: a sweep
 * reclaims slots whose definition has expired.
 */
class GradientBuffer {
  public:
    /**
     * @brief Create the backing buffer at the gradient descriptor binding.
     * @param backend Backend the buffer lives on.
     */
    void init(AmethystBackend &backend);

    /**
     * @brief Slot for a shared (static) gradient, allocating and recording it on first use.
     *
     * The slot is cached on the Gradient, so a resolved definition returns its slot directly.
     * @param grad Definition to resolve. Null returns Gradient::INVALID_SLOT.
     * @return Stable slot index, or Gradient::INVALID_SLOT if the buffer is exhausted.
     */
    uint32_t resolveShared(const std::shared_ptr<const Gradient> &grad);

    /**
     * @brief Upload records written since the last sync.
     * @param cmdBuffer Backend-native command buffer, forwarded to the arena upload.
     */
    void sync(void *cmdBuffer);

  private:
    /**
     * @brief 64-byte GPU gradient record, one cache line.
     *
     * Struct-of-arrays so the record packs tight. Stop positions are halves (two per word) and
     * stop colors are packed RGBA8. The header carries type and stop count.
     */
    struct GpuGradient {
        uint32_t header = 0; // type(8) | stopCount(8) | flags(8) | spare(8)
        float angle = 0.0f;
        uint32_t radialCenter = 0; // packHalf2x16(cx, cy)
        float radialRadius = 0.0f;
        uint32_t stopT[MAX_GRADIENT_STOPS / 2] = {};
        uint32_t stopColor[MAX_GRADIENT_STOPS] = {};
    };
    static_assert(sizeof(GpuGradient) == 64, "GpuGradient must be 64 bytes");

    static GpuGradient encode(const Gradient &grad);

    /** @brief Reclaim every slot whose gradient definition has expired. */
    void sweep();

    GpuArena m_arena;
    std::vector<GpuGradient> m_records;
    std::vector<ArenaBlock> m_blocks;
    std::vector<std::weak_ptr<const Gradient>> m_owners;
    DirtyRange m_dirty;
};

} // namespace Amethyst

#endif // AMETHYST__GRADIENT_BUFFER_H
