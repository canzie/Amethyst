#include "modules/glyph_buffer.h"

#include "logging/log.h"
#include "utils/am_assert.h"
#include "utils/packing.h"

#include <algorithm>
#include <cstring>

namespace Amethyst {

static constexpr uint32_t MIN_GLYPH_BLOCK = 16;
static constexpr uint32_t MIN_LINE_BLOCK = 4;

GlyphBuffer::GlyphBuffer()
{
    m_glyphs.resize(GLYPH_CAPACITY);
    m_lines.resize(LINE_CAPACITY);
    m_slices.resize(SLICE_CAPACITY);

    m_records.reserve(SLICE_CAPACITY);

    m_glyphAlloc.init(GLYPH_CAPACITY);
    m_lineAlloc.init(LINE_CAPACITY);
}

GlyphSliceHandle GlyphBuffer::createSlice()
{
    uint32_t id;
    if (!m_freeHandles.empty()) {
        id = m_freeHandles.back();
        m_freeHandles.pop_back();
        m_records[id] = SliceRecord{};
        m_records[id].alive = true;
    } else {
        id = static_cast<uint32_t>(m_records.size());
        if (id >= SLICE_CAPACITY) {
            AM_LOG_ERROR("GlyphBuffer slice table full ({} slices), cannot create slice", SLICE_CAPACITY);
            return GlyphSliceHandle{};
        }
        SliceRecord record;
        record.alive = true;
        m_records.push_back(record);
    }

    ++m_liveCount;
    return GlyphSliceHandle{id};
}

bool GlyphBuffer::ensureBlockCapacity(BlockAllocator &alloc, void *arena, uint32_t elemSize, Block &block, uint32_t needed,
                                      uint32_t minBlock, DirtyRange &dirty)
{
    if (needed <= block.capacity) {
        return true;
    }

    uint32_t target;
    if (block.capacity == 0) {
        target = std::max(needed, minBlock);
    } else {
        target = std::max(needed, block.capacity * 2);
    }

    if (block.offset == BlockAllocator::INVALID) {
        uint32_t off = alloc.alloc(target);
        if (off == BlockAllocator::INVALID) {
            return false;
        }
        block.offset = off;
        block.capacity = target;
        return true;
    }

    if (alloc.growInPlace(block.offset, block.capacity, target)) {
        block.capacity = target;
        return true;
    }

    uint32_t off = alloc.alloc(target);
    if (off == BlockAllocator::INVALID) {
        return false;
    }

    char *base = static_cast<char *>(arena);
    std::memmove(base + static_cast<size_t>(off) * elemSize, base + static_cast<size_t>(block.offset) * elemSize,
                 static_cast<size_t>(block.count) * elemSize);
    dirty.add(off, off + block.count);
    alloc.release(block.offset, block.capacity);
    block.offset = off;
    block.capacity = target;
    return true;
}

void GlyphBuffer::reserve(GlyphSliceHandle handle, uint32_t glyphCapacity)
{
    if (!handle.isValid() || handle.id >= m_records.size()) {
        return;
    }
    SliceRecord &rec = m_records[handle.id];
    if (!rec.alive) {
        return;
    }
    if (glyphCapacity == 0) {
        return;
    }

    if (!ensureBlockCapacity(m_glyphAlloc, m_glyphs.data(), sizeof(GlyphQuad), rec.glyph, glyphCapacity, MIN_GLYPH_BLOCK,
                             m_glyphDirty)) {
        compact();
        ensureBlockCapacity(m_glyphAlloc, m_glyphs.data(), sizeof(GlyphQuad), rec.glyph, glyphCapacity, MIN_GLYPH_BLOCK,
                            m_glyphDirty);
    }
}

void GlyphBuffer::updateSlice(GlyphSliceHandle handle, const GlyphQuad *glyphs, uint32_t glyphCount, const GlyphLine *lines,
                              uint32_t lineCount, float lineHeightPx)
{
    if (!handle.isValid() || handle.id >= m_records.size()) {
        return;
    }
    SliceRecord &rec = m_records[handle.id];
    if (!rec.alive) {
        return;
    }
    if (glyphCount == 0) {
        return;
    }

    AM_ASSERT(lineCount > 0 && lineCount < 65536, "GlyphBuffer::updateSlice lineCount out of range");

    if (!ensureBlockCapacity(m_glyphAlloc, m_glyphs.data(), sizeof(GlyphQuad), rec.glyph, glyphCount, MIN_GLYPH_BLOCK,
                             m_glyphDirty)) {
        compact();
        if (!ensureBlockCapacity(m_glyphAlloc, m_glyphs.data(), sizeof(GlyphQuad), rec.glyph, glyphCount, MIN_GLYPH_BLOCK,
                                 m_glyphDirty)) {
            AM_LOG_ERROR("GlyphBuffer glyph arena exhausted, dropping update of {} glyphs", glyphCount);
            return;
        }
    }

    if (!ensureBlockCapacity(m_lineAlloc, m_lines.data(), sizeof(GlyphLine), rec.line, lineCount, MIN_LINE_BLOCK, m_lineDirty)) {
        compact();
        if (!ensureBlockCapacity(m_lineAlloc, m_lines.data(), sizeof(GlyphLine), rec.line, lineCount, MIN_LINE_BLOCK,
                                 m_lineDirty)) {
            AM_LOG_ERROR("GlyphBuffer line arena exhausted, dropping update of {} lines", lineCount);
            return;
        }
    }

    std::memcpy(&m_glyphs[rec.glyph.offset], glyphs, static_cast<size_t>(glyphCount) * sizeof(GlyphQuad));
    rec.glyph.count = glyphCount;

    std::memcpy(&m_lines[rec.line.offset], lines, static_cast<size_t>(lineCount) * sizeof(GlyphLine));
    rec.line.count = lineCount;

    m_glyphDirty.add(rec.glyph.offset, rec.glyph.offset + glyphCount);
    m_lineDirty.add(rec.line.offset, rec.line.offset + lineCount);

    m_slices[handle.id] = GlyphSlice{rec.glyph.offset, rec.line.offset,
                                     packU16x2(static_cast<uint16_t>(lineCount), packFloatToHalf(lineHeightPx)), 0};
    m_sliceDirty.add(handle.id, handle.id + 1);
}

void GlyphBuffer::destroySlice(GlyphSliceHandle handle)
{
    if (!handle.isValid() || handle.id >= m_records.size()) {
        return;
    }
    SliceRecord &rec = m_records[handle.id];
    if (!rec.alive) {
        return;
    }

    if (rec.glyph.offset != BlockAllocator::INVALID) {
        m_glyphAlloc.release(rec.glyph.offset, rec.glyph.capacity);
    }
    if (rec.line.offset != BlockAllocator::INVALID) {
        m_lineAlloc.release(rec.line.offset, rec.line.capacity);
    }

    rec = SliceRecord{};
    m_freeHandles.push_back(handle.id);
    --m_liveCount;
}

void GlyphBuffer::compact()
{
    std::vector<uint32_t> order;
    order.reserve(m_records.size());

    for (uint32_t id = 0; id < m_records.size(); ++id) {
        if (m_records[id].alive && m_records[id].glyph.offset != BlockAllocator::INVALID) {
            order.push_back(id);
        }
    }
    std::sort(order.begin(), order.end(),
              [this](uint32_t a, uint32_t b) { return m_records[a].glyph.offset < m_records[b].glyph.offset; });

    uint32_t cursor = 0;
    for (uint32_t id : order) {
        Block &block = m_records[id].glyph;
        if (block.offset != cursor) {
            std::memmove(&m_glyphs[cursor], &m_glyphs[block.offset], static_cast<size_t>(block.count) * sizeof(GlyphQuad));
            block.offset = cursor;
            m_slices[id].firstGlyph = cursor;
            m_sliceDirty.add(id, id + 1);
            m_glyphDirty.add(cursor, cursor + block.count);
        }
        cursor += block.capacity;
    }
    m_glyphAlloc.rebuildFreelist(cursor);

    order.clear();
    for (uint32_t id = 0; id < m_records.size(); ++id) {
        if (m_records[id].alive && m_records[id].line.offset != BlockAllocator::INVALID) {
            order.push_back(id);
        }
    }
    std::sort(order.begin(), order.end(),
              [this](uint32_t a, uint32_t b) { return m_records[a].line.offset < m_records[b].line.offset; });

    cursor = 0;
    for (uint32_t id : order) {
        Block &block = m_records[id].line;
        if (block.offset != cursor) {
            std::memmove(&m_lines[cursor], &m_lines[block.offset], static_cast<size_t>(block.count) * sizeof(GlyphLine));
            block.offset = cursor;
            m_slices[id].firstLine = cursor;
            m_sliceDirty.add(id, id + 1);
            m_lineDirty.add(cursor, cursor + block.count);
        }
        cursor += block.capacity;
    }
    m_lineAlloc.rebuildFreelist(cursor);
}

void GlyphBuffer::clearDirty()
{
    m_glyphDirty.clear();
    m_lineDirty.clear();
    m_sliceDirty.clear();
}

} // namespace Amethyst
