#include "modules/atlas_packer.h"
#include "modules/color.h"
#include "utils/free_list.h"
#include "utils/packing.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace Amethyst;

// ---- FreeList ----

TEST(FreeList, InsertReturnsSequentialIds)
{
    FreeList<int> list;
    EXPECT_EQ(list.insert(10), 0u);
    EXPECT_EQ(list.insert(20), 1u);
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(*list.tryGet(1), 20);
}

TEST(FreeList, EraseKeepsOtherIdsAndReusesTheSlot)
{
    FreeList<int> list;
    list.insert(1);
    list.insert(2);
    list.insert(3);
    list.erase(1);
    EXPECT_EQ(list.tryGet(1), nullptr);
    EXPECT_EQ(*list.tryGet(2), 3);
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.insert(4), 1u);
    EXPECT_EQ(list.slotCount(), 3u);
}

TEST(FreeList, DoubleEraseAndOutOfRangeAreNoOps)
{
    FreeList<int> list;
    list.insert(1);
    list.erase(0);
    list.erase(0);
    list.erase(42);
    list.erase(FreeList<int>::INVALID);
    EXPECT_TRUE(list.empty());
    // a double erase must not push the slot twice, or two inserts would share it
    uint32_t a = list.insert(5);
    uint32_t b = list.insert(6);
    EXPECT_NE(a, b);
    EXPECT_EQ(list.tryGet(FreeList<int>::INVALID), nullptr);
}

// ---- packColor ----

TEST(PackColor, RoundsToNearest)
{
    // 0.5 * 255 = 127.5, truncation gave 127
    EXPECT_EQ(packColor(Color4(0.5f, 0.0f, 0.0f, 0.0f)) & 0xFFu, 128u);
    EXPECT_EQ(packColor(Color4(1.0f, 1.0f, 1.0f, 1.0f)), 0xFFFFFFFFu);
    EXPECT_EQ(packColor(Color4(0.0f, 0.0f, 0.0f, 0.0f)), 0u);
}

TEST(PackColor, ClampsOvershootInsteadOfWrapping)
{
    // 1.004 * 255 = 256.02, which used to wrap to 0 through & 0xFF
    EXPECT_EQ(packColor(Color4(1.004f, 2.0f, 1.0f, 1.0f)), 0xFFFFFFFFu);
}

TEST(PackColor, ClampsNegativesAndNanToZero)
{
    float nan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(packColor(Color4(-0.5f, -100.0f, nan, 1.0f)), 0xFF000000u);
}

TEST(PackColor, IntegerChannelsRoundTrip)
{
    for (int v = 0; v <= 255; ++v) {
        EXPECT_EQ(packColor(v, v, v, v) & 0xFFu, static_cast<uint32_t>(v));
    }
}

TEST(PackColor, ChannelOrderIsRgbaLowToHigh)
{
    EXPECT_EQ(packColor(Color4(1.0f, 0.0f, 0.0f, 0.0f)), 0x000000FFu);
    EXPECT_EQ(packColor(Color4(0.0f, 1.0f, 0.0f, 0.0f)), 0x0000FF00u);
    EXPECT_EQ(packColor(Color4(0.0f, 0.0f, 1.0f, 0.0f)), 0x00FF0000u);
    EXPECT_EQ(packColor(Color4(0.0f, 0.0f, 0.0f, 1.0f)), 0xFF000000u);
}

TEST(Packing, ClampToU16RoundsAndClamps)
{
    EXPECT_EQ(clampToU16(-3.0f), 0);
    EXPECT_EQ(clampToU16(1.5f), 2);
    EXPECT_EQ(clampToU16(70000.0f), 65535);
}

// ---- atlas packers ----

static bool s_overlaps(const AtlasRegion &a, const AtlasRegion &b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

template <typename Packer> static void s_checkNoOverlapAndInBounds(Packer &packer)
{
    std::vector<AtlasRegion> placed;
    uint32_t sizes[] = {7, 13, 1, 30, 5, 64, 2, 17};
    for (int i = 0; i < 400; ++i) {
        uint32_t w = sizes[i % 8];
        uint32_t h = sizes[(i * 3 + 1) % 8];
        auto region = packer.packRect(w, h);
        if (!region.has_value()) {
            continue;
        }
        EXPECT_LE(region->x + region->width, packer.getWidth());
        EXPECT_LE(region->y + region->height, packer.getHeight());
        for (const AtlasRegion &other : placed) {
            ASSERT_FALSE(s_overlaps(*region, other));
        }
        placed.push_back(*region);
    }
    EXPECT_FALSE(placed.empty());
}

TEST(ShelfPacker, ExactFitFillsTheAtlas)
{
    ShelfPacker packer(64, 64);
    ASSERT_TRUE(packer.packRect(32, 32).has_value());
    ASSERT_TRUE(packer.packRect(32, 32).has_value());
    ASSERT_TRUE(packer.packRect(32, 32).has_value());
    auto last = packer.packRect(32, 32);
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->x, 32);
    EXPECT_EQ(last->y, 32);
    EXPECT_FALSE(packer.packRect(1, 1).has_value());
}

TEST(ShelfPacker, RejectsOversizedRects)
{
    ShelfPacker packer(64, 64);
    EXPECT_FALSE(packer.packRect(65, 1).has_value());
    EXPECT_FALSE(packer.packRect(1, 65).has_value());
    EXPECT_TRUE(packer.packRect(64, 64).has_value());
}

TEST(ShelfPacker, FailedRectDoesNotAbandonTheOpenRow)
{
    ShelfPacker packer(64, 64);
    ASSERT_TRUE(packer.packRect(40, 40).has_value());
    // does not fit next to the first rect nor in a new row below it
    EXPECT_FALSE(packer.packRect(40, 30).has_value());
    // the space to the right of the first rect must still be usable
    auto region = packer.packRect(24, 10);
    ASSERT_TRUE(region.has_value());
    EXPECT_EQ(region->x, 40);
    EXPECT_EQ(region->y, 0);
}

TEST(ShelfPacker, RegionsNeverOverlap)
{
    ShelfPacker packer(256, 256);
    s_checkNoOverlapAndInBounds(packer);
}

TEST(SkylinePacker, ExactFitFillsTheAtlas)
{
    SkylinePacker packer(64, 64);
    for (int i = 0; i < 4; ++i) {
        ASSERT_TRUE(packer.packRect(32, 32).has_value());
    }
    EXPECT_FALSE(packer.packRect(1, 1).has_value());
}

TEST(SkylinePacker, RejectsZeroWidthAndOversizedRects)
{
    SkylinePacker packer(64, 64);
    // a zero width skipped the height check and could return a region past the bottom
    EXPECT_FALSE(packer.packRect(0, 100).has_value());
    EXPECT_FALSE(packer.packRect(65, 1).has_value());
    EXPECT_FALSE(packer.packRect(1, 65).has_value());
}

TEST(SkylinePacker, RegionsNeverOverlap)
{
    SkylinePacker packer(256, 256);
    s_checkNoOverlapAndInBounds(packer);
}

TEST(SkylinePacker, ResetStartsOver)
{
    SkylinePacker packer(32, 32);
    ASSERT_TRUE(packer.packRect(32, 32).has_value());
    packer.reset();
    auto region = packer.packRect(32, 32);
    ASSERT_TRUE(region.has_value());
    EXPECT_EQ(region->x, 0);
    EXPECT_EQ(region->y, 0);
}
