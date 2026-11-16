#include "common/fake_backend.h"

#include "rendering/gpu_arena.h"

#include <gtest/gtest.h>

using namespace Amethyst;
using AmethystTest::FakeBackend;

// ---- BlockAllocator ----

TEST(BlockAllocator, ExactFitConsumesTheWholeSpan)
{
    BlockAllocator alloc;
    alloc.init(16);
    EXPECT_EQ(alloc.alloc(16), 0u);
    EXPECT_EQ(alloc.alloc(1), BlockAllocator::INVALID);
}

TEST(BlockAllocator, RejectsZeroAndOversizedRequests)
{
    BlockAllocator alloc;
    alloc.init(8);
    EXPECT_EQ(alloc.alloc(0), BlockAllocator::INVALID);
    EXPECT_EQ(alloc.alloc(9), BlockAllocator::INVALID);
    EXPECT_EQ(alloc.alloc(8), 0u);
}

TEST(BlockAllocator, ReleaseCoalescesWithBothNeighbours)
{
    BlockAllocator alloc;
    alloc.init(12);
    uint32_t a = alloc.alloc(4);
    uint32_t b = alloc.alloc(4);
    uint32_t c = alloc.alloc(4);
    alloc.release(a, 4);
    alloc.release(c, 4);
    alloc.release(b, 4);
    // only a single 12-slot span can satisfy this
    EXPECT_EQ(alloc.alloc(12), 0u);
}

TEST(BlockAllocator, GrowInPlaceUsesTheFollowingSpanExactly)
{
    BlockAllocator alloc;
    alloc.init(8);
    uint32_t a = alloc.alloc(4);
    EXPECT_TRUE(alloc.growInPlace(a, 4, 8));
    EXPECT_EQ(alloc.alloc(1), BlockAllocator::INVALID);
}

TEST(BlockAllocator, GrowInPlaceFailsWhenTheNeighbourIsTaken)
{
    BlockAllocator alloc;
    alloc.init(16);
    uint32_t a = alloc.alloc(4);
    alloc.alloc(4);
    EXPECT_FALSE(alloc.growInPlace(a, 4, 6));
}

TEST(BlockAllocator, GrowExtendsTheTailSpan)
{
    BlockAllocator alloc;
    alloc.init(8);
    alloc.alloc(6);
    alloc.grow(16);
    EXPECT_EQ(alloc.capacity(), 16u);
    EXPECT_EQ(alloc.alloc(10), 6u);
}

TEST(BlockAllocator, RebuildFreelistLeavesOnlyTheTail)
{
    BlockAllocator alloc;
    alloc.init(10);
    alloc.rebuildFreelist(10);
    EXPECT_EQ(alloc.alloc(1), BlockAllocator::INVALID);
    alloc.rebuildFreelist(4);
    EXPECT_EQ(alloc.alloc(6), 4u);
}

// ---- GpuArena ----

static AmBufferDesc s_desc(size_t bytes)
{
    return {bytes, AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 0};
}

TEST(GpuArena, AllocatesFromTheTailUntilFull)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(256), GrowthPolicy::fixed());

    ArenaBlock a = arena.alloc(128);
    ArenaBlock b = arena.alloc(128);
    EXPECT_EQ(a.offset, 0u);
    EXPECT_EQ(b.offset, 128u);
    EXPECT_FALSE(arena.alloc(1).isValid());
}

TEST(GpuArena, FreedBlockIsNotReusedWhileFramesAreInFlight)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(256), GrowthPolicy::doubleUntil(1024));

    ArenaBlock a = arena.alloc(128);
    arena.alloc(128);
    arena.free(a);

    // same frame: the GPU may still be reading a, so the arena must hand out fresh memory
    ArenaBlock b = arena.alloc(128);
    ASSERT_TRUE(b.isValid());
    EXPECT_NE(b.offset, a.offset);
    EXPECT_EQ(arena.retiredBytes(), 128u);

    arena.advanceFrame();
    EXPECT_EQ(arena.retiredBytes(), 128u);
    arena.advanceFrame();
    EXPECT_EQ(arena.retiredBytes(), 0u);

    ArenaBlock c = arena.alloc(128);
    EXPECT_EQ(c.offset, a.offset);
}

TEST(GpuArena, ZeroRetireLatencyReleasesImmediately)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(256), GrowthPolicy::fixed());
    arena.setRetireLatency(0);

    ArenaBlock a = arena.alloc(256);
    arena.free(a);
    EXPECT_EQ(arena.alloc(256).offset, 0u);
}

TEST(GpuArena, FreeCoalescesWithBothNeighboursInAnyOrder)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(300), GrowthPolicy::fixed());
    arena.setRetireLatency(0);

    ArenaBlock a = arena.alloc(100);
    ArenaBlock b = arena.alloc(100);
    ArenaBlock c = arena.alloc(100);

    // free the outer two first, then the middle one: it has to merge with both
    arena.free(c);
    arena.free(a);
    arena.free(b);
    EXPECT_EQ(arena.freeBlockCount(), 1u);
    EXPECT_EQ(arena.freeBytes(), 300u);

    ArenaBlock all = arena.alloc(300);
    ASSERT_TRUE(all.isValid());
    EXPECT_EQ(all.offset, 0u);
}

TEST(GpuArena, GrowsThroughTheBackendWhenThePolicyAllows)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(64), GrowthPolicy::doubleUntil(512));

    arena.alloc(64);
    ArenaBlock big = arena.alloc(200);
    ASSERT_TRUE(big.isValid());
    EXPECT_EQ(big.offset, 64u);
    EXPECT_GE(arena.capacity(), 264u);
    EXPECT_LE(arena.capacity(), 512u);
    EXPECT_EQ(backend.growCount, 1u);
}

TEST(GpuArena, GrowthStopsAtThePolicyCap)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(64), GrowthPolicy::doubleUntil(256));
    EXPECT_FALSE(arena.alloc(257).isValid());
    EXPECT_TRUE(arena.alloc(256).isValid());
}

TEST(GpuArena, FailedBackendGrowthReturnsAnInvalidBlock)
{
    FakeBackend backend;
    backend.allowGrow = false;
    GpuArena arena;
    arena.init(backend, s_desc(64), GrowthPolicy::doubleUntil(1024));
    EXPECT_FALSE(arena.alloc(65).isValid());
    EXPECT_TRUE(arena.alloc(64).isValid());
}

TEST(GpuArena, ZeroCapacityArenaDoesNotSpinWhenGrowing)
{
    FakeBackend backend;
    GpuArena arena;
    arena.init(backend, s_desc(0), GrowthPolicy::doubleUntil(64));
    ArenaBlock block = arena.alloc(32);
    ASSERT_TRUE(block.isValid());
    EXPECT_EQ(block.offset, 0u);
}

TEST(GpuArena, InvalidBufferNeverAllocates)
{
    FakeBackend backend;
    backend.failBindings.insert(0);
    GpuArena arena;
    arena.init(backend, s_desc(64), GrowthPolicy::fixed());
    EXPECT_FALSE(arena.alloc(16).isValid());
}

TEST(DirtyRange, AddsAndClears)
{
    DirtyRange range;
    EXPECT_TRUE(range.empty());
    range.add(4, 8);
    range.add(2, 3);
    EXPECT_EQ(range.lo, 2u);
    EXPECT_EQ(range.hi, 8u);
    range.clear();
    EXPECT_TRUE(range.empty());
}
