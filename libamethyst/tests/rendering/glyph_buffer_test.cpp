#include "modules/glyph_buffer.h"

#include <gtest/gtest.h>

#include <vector>

using namespace Amethyst;

namespace {

struct Text {
    std::vector<GlyphQuad> glyphs;
    std::vector<GlyphLine> lines;
};

/** @brief n glyphs tagged with `tag` in their colour, split over `lineCount` lines. */
Text s_text(uint32_t n, uint32_t tag, uint32_t lineCount = 1)
{
    Text text;
    text.glyphs.resize(n);
    for (GlyphQuad &quad : text.glyphs) {
        quad.color = tag;
    }
    text.lines.resize(lineCount, GlyphLine{0, n});
    return text;
}

void s_update(GlyphBuffer &buffer, GlyphSliceHandle handle, const Text &text)
{
    buffer.updateSlice(handle, text.glyphs.data(), static_cast<uint32_t>(text.glyphs.size()), text.lines.data(),
                       static_cast<uint32_t>(text.lines.size()), 16.0f);
}

uint32_t s_lineCount(const GlyphSlice &slice)
{
    return slice.packed & 0xFFFFu;
}

} // namespace

TEST(GlyphBuffer, UpdateWritesGlyphsAndSliceEntry)
{
    GlyphBuffer buffer;
    GlyphSliceHandle a = buffer.createSlice();
    ASSERT_TRUE(a.isValid());
    s_update(buffer, a, s_text(5, 0xA, 2));

    const GlyphSlice &entry = buffer.sliceData()[a.id];
    EXPECT_EQ(s_lineCount(entry), 2u);
    EXPECT_EQ(buffer.glyphData()[entry.firstGlyph].color, 0xAu);
    EXPECT_FALSE(buffer.glyphDirty().empty());
    EXPECT_FALSE(buffer.sliceDirty().empty());

    buffer.clearDirty();
    EXPECT_TRUE(buffer.glyphDirty().empty());
}

TEST(GlyphBuffer, ReserveRelocationRepointsTheSliceEntry)
{
    GlyphBuffer buffer;
    GlyphSliceHandle a = buffer.createSlice();
    GlyphSliceHandle b = buffer.createSlice();
    s_update(buffer, a, s_text(16, 0xA));
    s_update(buffer, b, s_text(16, 0xB));
    uint32_t before = buffer.sliceData()[a.id].firstGlyph;

    // b sits right after a, so a cannot grow in place and moves, releasing its old range
    buffer.reserve(a, 64);
    uint32_t after = buffer.sliceData()[a.id].firstGlyph;
    EXPECT_NE(after, before);
    EXPECT_EQ(buffer.glyphData()[after].color, 0xAu);

    // a new slice takes the released range; a stale entry for a would now show its glyphs
    GlyphSliceHandle c = buffer.createSlice();
    s_update(buffer, c, s_text(16, 0xC));
    EXPECT_NE(buffer.sliceData()[a.id].firstGlyph, buffer.sliceData()[c.id].firstGlyph);
    EXPECT_EQ(buffer.glyphData()[buffer.sliceData()[a.id].firstGlyph].color, 0xAu);
}

TEST(GlyphBuffer, FailedLineGrowthStillRepointsAMovedGlyphBlock)
{
    GlyphBuffer buffer;
    GlyphSliceHandle hole = buffer.createSlice();
    GlyphSliceHandle a = buffer.createSlice();
    GlyphSliceHandle b = buffer.createSlice();
    GlyphSliceHandle filler = buffer.createSlice();
    s_update(buffer, hole, s_text(40, 0x0, 1));
    s_update(buffer, a, s_text(16, 0xA, 1));
    s_update(buffer, b, s_text(16, 0xB, 1));
    // takes every remaining line slot
    s_update(buffer, filler, s_text(16, 0xF, GlyphBuffer::LINE_CAPACITY - 3));
    buffer.destroySlice(hole);

    // a's glyph block cannot grow in place (b follows it), so it moves into the hole at the
    // front, which compaction then leaves where it is; the line block cannot grow at all, so
    // the update is dropped, but a's entry must follow its glyphs instead of pointing at the
    // released range that compaction hands to b
    s_update(buffer, a, s_text(40, 0xA2, 10));
    const GlyphSlice &entry = buffer.sliceData()[a.id];
    EXPECT_EQ(s_lineCount(entry), 1u);
    EXPECT_EQ(buffer.glyphData()[entry.firstGlyph].color, 0xAu);
    EXPECT_NE(entry.firstGlyph, buffer.sliceData()[b.id].firstGlyph);
}

TEST(GlyphBuffer, DestroyClearsTheEntrySoAReusedHandleDrawsNothing)
{
    GlyphBuffer buffer;
    GlyphSliceHandle a = buffer.createSlice();
    s_update(buffer, a, s_text(8, 0xA, 3));
    buffer.clearDirty();

    buffer.destroySlice(a);
    EXPECT_FALSE(buffer.hasLiveSlices());
    EXPECT_FALSE(buffer.sliceDirty().empty());

    GlyphSliceHandle reused = buffer.createSlice();
    ASSERT_EQ(reused.id, a.id);
    // the shader treats a zero line count as "nothing to draw"
    EXPECT_EQ(s_lineCount(buffer.sliceData()[reused.id]), 0u);
}

TEST(GlyphBuffer, SliceTableFullReturnsInvalidHandle)
{
    GlyphBuffer buffer;
    for (uint32_t i = 0; i < GlyphBuffer::SLICE_CAPACITY; ++i) {
        ASSERT_TRUE(buffer.createSlice().isValid());
    }
    EXPECT_FALSE(buffer.createSlice().isValid());
}

TEST(GlyphBuffer, CompactPacksBlocksAndRepointsEntries)
{
    GlyphBuffer buffer;
    GlyphSliceHandle a = buffer.createSlice();
    GlyphSliceHandle b = buffer.createSlice();
    s_update(buffer, a, s_text(16, 0xA));
    s_update(buffer, b, s_text(16, 0xB));
    buffer.destroySlice(a);
    buffer.clearDirty();

    buffer.compact();
    const GlyphSlice &entry = buffer.sliceData()[b.id];
    EXPECT_EQ(entry.firstGlyph, 0u);
    EXPECT_EQ(buffer.glyphData()[0].color, 0xBu);
    EXPECT_FALSE(buffer.sliceDirty().empty());
}

TEST(GlyphBuffer, GlyphArenaGrowsForLargeSlices)
{
    GlyphBuffer buffer;
    GlyphSliceHandle a = buffer.createSlice();
    s_update(buffer, a, s_text(GlyphBuffer::GLYPH_FLOOR + 1, 0xA));
    EXPECT_GT(buffer.glyphCapacity(), GlyphBuffer::GLYPH_FLOOR);
    EXPECT_EQ(buffer.glyphData()[buffer.sliceData()[a.id].firstGlyph].color, 0xAu);
}

TEST(GlyphBuffer, InvalidAndDeadHandlesAreIgnored)
{
    GlyphBuffer buffer;
    Text text = s_text(4, 0xA);
    s_update(buffer, GlyphSliceHandle{}, text);
    s_update(buffer, GlyphSliceHandle{7}, text);
    buffer.reserve(GlyphSliceHandle{}, 10);
    buffer.destroySlice(GlyphSliceHandle{});
    EXPECT_TRUE(buffer.glyphDirty().empty());
}
