/**
 * @file glyph_buffer.h
 * @brief Per-registry side buffers for batched text: glyph quads, line ranges, slice table
 */

#ifndef AMETHYST__GLYPH_BUFFER_H
#define AMETHYST__GLYPH_BUFFER_H

#include "rendering/gpu_arena.h"

#include <cstdint>
#include <vector>

namespace Amethyst {

/**
 * @brief One glyph's screen rect, atlas region, colour and atlas page for batched text.
 *
 * Positions are u16 pixels relative to the label bbox top-left; UVs are u16 atlas pixel
 * coordinates. Both are stored as packed half2 pairs (low 16 = x, high 16 = y).
 */
struct GlyphQuad {
    uint32_t posMin = 0;
    uint32_t posMax = 0;
    uint32_t uvMin = 0;
    uint32_t uvMax = 0;
    uint32_t color = 0xFFFFFFFF;
    uint32_t page = 0;
    uint32_t flags = 0;
    uint32_t unused = 0;
};

static_assert(sizeof(GlyphQuad) == 32, "GlyphQuad must stay one memory sector wide");

/**
 * @brief A run of glyphs forming one visual line, relative to the slice's glyph run.
 */
struct GlyphLine {
    uint32_t glyphStart = 0;
    uint32_t glyphCount = 0;
};

/**
 * @brief GPU indirection entry: maps a stable handle to a slice's current location.
 *
 * The vertex shader reads this once per instance and forwards the fields as flat varyings,
 * so a slice's data can move (growth, compaction) by rewriting only this entry.
 */
struct GlyphSlice {
    uint32_t firstGlyph = 0;
    uint32_t firstLine = 0;
    uint32_t packed = 0; // lineCount (low 16) | lineHeightPx as half (high 16)
    uint32_t pad = 0;
};

struct GlyphSliceHandle {
    uint32_t id = UINT32_MAX;
    bool isValid() const { return id != UINT32_MAX; }
};

/**
 * @brief Owns the three side buffers for batched text in one GeometryRegistry.
 *
 * Created lazily by a registry on its first slice and destroyed with it. A slice is a
 * contiguous run of GlyphQuads plus its per-line ranges, addressed through a stable handle
 * into the slice table.
 *
 * The glyph arena starts at GLYPH_FLOOR and doubles up to GLYPH_MAX, which is past what a
 * viewport of visible text can hold at any resolution and font size. Reaching it means
 * something laid out more than it can show. Line and slice capacities are fixed, since a
 * run of visible rows needs far fewer of both than of glyphs.
 */
class GlyphBuffer {
  public:
    static constexpr uint32_t GLYPH_FLOOR = 1u << 15;
    static constexpr uint32_t GLYPH_MAX = 1u << 18;
    static constexpr uint32_t LINE_CAPACITY = 1u << 13;
    static constexpr uint32_t SLICE_CAPACITY = 1u << 11;

    GlyphBuffer();

    /**
     * @brief Reserve a stable slice handle. No glyph storage is allocated yet.
     * @return A valid handle, or an invalid handle if the slice table is full.
     */
    GlyphSliceHandle createSlice();

    /**
     * @brief Pre-size a slice's glyph block to avoid repeated growth, e.g. for text inputs.
     * @param handle Slice to pre-size.
     * @param glyphCapacity Number of glyph slots to reserve up front.
     */
    void reserve(GlyphSliceHandle handle, uint32_t glyphCapacity);

    /**
     * @brief Fill a slice with glyph and line data, growing its blocks if needed.
     *
     * Within existing capacity this only rewrites the used range and the slice entry; the
     * block bases do not move, so the owning instance never changes.
     *
     * @param handle Slice to fill.
     * @param glyphs Glyph quads to copy in, in reading order.
     * @param glyphCount Number of glyph quads.
     * @param lines Per-line ranges into the glyph run.
     * @param lineCount Number of lines.
     * @param lineHeightPx Uniform line height in pixels, packed into the slice entry.
     */
    void updateSlice(GlyphSliceHandle handle, const GlyphQuad *glyphs, uint32_t glyphCount, const GlyphLine *lines,
                     uint32_t lineCount, float lineHeightPx);

    /**
     * @brief Free a slice's blocks and return its handle to the pool.
     * @param handle Slice to destroy.
     */
    void destroySlice(GlyphSliceHandle handle);

    /**
     * @brief Defragment both arenas, packing live blocks to the front and rewriting moved
     *        slice entries. Holders and instances are unaffected.
     */
    void compact();

    /**
     * @brief Pointer to the glyph arena mirror for GPU upload.
     * @return Base pointer to the glyph quad array.
     */
    const GlyphQuad *glyphData() const { return m_glyphs.data(); }

    /**
     * @brief Pointer to the line arena mirror for GPU upload.
     * @return Base pointer to the glyph line array.
     */
    const GlyphLine *lineData() const { return m_lines.data(); }

    /**
     * @brief Pointer to the slice table mirror for GPU upload.
     * @return Base pointer to the slice array.
     */
    const GlyphSlice *sliceData() const { return m_slices.data(); }

    /**
     * @brief Glyph slots written since the last clearDirty.
     * @return Dirty interval into the glyph arena.
     */
    DirtyRange glyphDirty() const { return m_glyphDirty; }

    /**
     * @brief Line slots written since the last clearDirty.
     * @return Dirty interval into the line arena.
     */
    DirtyRange lineDirty() const { return m_lineDirty; }

    /**
     * @brief Slice entries written since the last clearDirty.
     * @return Dirty interval into the slice table.
     */
    DirtyRange sliceDirty() const { return m_sliceDirty; }

    /** @brief Reset all dirty intervals once the backend has uploaded them. */
    void clearDirty();

    /**
     * @brief Whether any slice is currently live.
     * @return True if at least one slice is allocated.
     */
    bool hasLiveSlices() const { return m_liveCount > 0; }

    /**
     * @brief Slots the glyph arena currently spans, which consumers must match to upload it.
     * @return Glyph arena capacity in quads
     */
    uint32_t glyphCapacity() const { return m_glyphAlloc.capacity(); }

  private:
    struct Block {
        uint32_t offset = BlockAllocator::INVALID;
        uint32_t capacity = 0;
        uint32_t count = 0;
    };

    struct SliceRecord {
        Block glyph;
        Block line;
        bool alive = false;
    };

    /**
     * @brief Ensure a block has room for `needed` slots, growing or relocating as required.
     * @param alloc Arena allocator owning the block.
     * @param arena Base pointer of the mirror the block indexes into.
     * @param elemSize Size in bytes of one arena element.
     * @param block Block to grow, updated in place.
     * @param needed Required slot count.
     * @param minBlock Minimum size when first allocating an empty block.
     * @param dirty Dirty interval to extend if relocation moves existing data.
     * @return True on success, false if the arena could not satisfy the request.
     */
    bool ensureBlockCapacity(BlockAllocator &alloc, void *arena, uint32_t elemSize, Block &block, uint32_t needed,
                             uint32_t minBlock, DirtyRange &dirty);

    /**
     * @brief Fit a slice's glyphs, compacting and then enlarging the arena if it will not fit.
     * @param block Glyph block to fit, updated in place
     * @param needed Glyph slots the slice requires
     * @return True once the block holds `needed` slots, false at the ceiling
     */
    bool fitGlyphBlock(Block &block, uint32_t needed);

    /**
     * @brief Double the glyph arena until it spans `needed` slots, capped at GLYPH_MAX.
     * @param needed Slots the arena must span
     * @return True if the arena grew, false if it is already at the ceiling
     */
    bool growGlyphArena(uint32_t needed);

    std::vector<GlyphQuad> m_glyphs;
    std::vector<GlyphLine> m_lines;
    std::vector<GlyphSlice> m_slices;

    std::vector<SliceRecord> m_records;
    std::vector<uint32_t> m_freeHandles;

    BlockAllocator m_glyphAlloc;
    BlockAllocator m_lineAlloc;

    DirtyRange m_glyphDirty;
    DirtyRange m_lineDirty;
    DirtyRange m_sliceDirty;

    uint32_t m_liveCount = 0;
};

} // namespace Amethyst

#endif // AMETHYST__GLYPH_BUFFER_H
