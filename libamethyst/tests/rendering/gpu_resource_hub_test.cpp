#include "common/fake_backend.h"

#include "components/window.h"
#include "modules/color.h"
#include "modules/glyph_buffer.h"
#include "modules/gradient_buffer.h"
#include "rendering/geometry_registry.h"
#include "rendering/gpu_resource_hub.h"
#include "rendering/instance_data.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace Amethyst;
using AmethystTest::FakeBackend;

namespace {

constexpr uint32_t GLYPH_BINDING = 2;
constexpr size_t INITIAL_REGISTRY_INSTANCES = 256;

void s_submit(GeometryRegistry &registry, uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i) {
        registry.submit(InstanceData{});
    }
}

GlyphSliceHandle s_addText(GeometryRegistry &registry, uint32_t glyphCount)
{
    GlyphBuffer &buffer = registry.glyphBuffer();
    GlyphSliceHandle handle = buffer.createSlice();
    std::vector<GlyphQuad> glyphs(glyphCount);
    GlyphLine line{0, glyphCount};
    buffer.updateSlice(handle, glyphs.data(), glyphCount, &line, 1, 16.0f);

    InstanceData inst{};
    inst.setPrimitiveType(PRIMITIVE_TEXT);
    inst.setGlyphSlice(handle.id);
    registry.submit(inst);
    return handle;
}

const FrameDrawEntry *s_entryFor(const FrameDrawList &list, uint32_t firstInstanceAtLeast = 0)
{
    for (const FrameDrawEntry &entry : list.entries) {
        if (entry.firstInstance >= firstInstanceAtLeast) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace

// The hub must outlive the windows (registries report their destruction to it), so every test
// declares the hub first.

TEST(GpuResourceHub, DrawsOneEntryPerRegistryWithInstances)
{
    FakeBackend backend;
    GpuResourceHub hub;
    hub.init(backend);
    auto window = std::make_unique<Window>();

    s_submit(*window->geometryRegistry(), 3);
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, window.get());

    const FrameDrawList &list = hub.drawList(window.get());
    ASSERT_EQ(list.entries.size(), 1u);
    EXPECT_EQ(list.entries[0].instanceCount, 3u);
    // no glyph buffer: no text bases
    EXPECT_EQ(list.entries[0].sliceBase, FrameDrawEntry::NO_TEXT);
    EXPECT_EQ(backend.outOfBoundsUploads, 0u);
}

TEST(GpuResourceHub, TextBasesPointAtTheRegistrysBlocks)
{
    FakeBackend backend;
    GpuResourceHub hub;
    hub.init(backend);
    auto window = std::make_unique<Window>();

    s_addText(*window->geometryRegistry(), 4);
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, window.get());

    const FrameDrawList &list = hub.drawList(window.get());
    ASSERT_EQ(list.entries.size(), 1u);
    EXPECT_NE(list.entries[0].glyphBase, FrameDrawEntry::NO_TEXT);
    EXPECT_NE(list.entries[0].lineBase, FrameDrawEntry::NO_TEXT);
    EXPECT_NE(list.entries[0].sliceBase, FrameDrawEntry::NO_TEXT);
    EXPECT_TRUE(window->geometryRegistry()->getGlyphBuffer()->glyphDirty().empty());
}

TEST(GpuResourceHub, FailedTextAllocationDrawsNoTextInsteadOfAliasing)
{
    FakeBackend backend;
    backend.failBindings.insert(GLYPH_BINDING);
    GpuResourceHub hub;
    hub.init(backend);
    auto window = std::make_unique<Window>();

    s_addText(*window->geometryRegistry(), 4);
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, window.get());

    // bases of 0 would make this registry render whichever registry owns the arena front
    const FrameDrawList &list = hub.drawList(window.get());
    ASSERT_EQ(list.entries.size(), 1u);
    EXPECT_EQ(list.entries[0].instanceCount, 1u);
    EXPECT_EQ(list.entries[0].glyphBase, FrameDrawEntry::NO_TEXT);
    EXPECT_EQ(list.entries[0].lineBase, FrameDrawEntry::NO_TEXT);
    EXPECT_EQ(list.entries[0].sliceBase, FrameDrawEntry::NO_TEXT);
}

TEST(GpuResourceHub, FailedGlyphBlockGrowthKeepsTheUpdateDirtyForRetry)
{
    FakeBackend backend;
    backend.allowGrow = false;
    GpuResourceHub hub;
    hub.init(backend);
    auto window = std::make_unique<Window>();
    GeometryRegistry &registry = *window->geometryRegistry();

    // a slice this size doubles the registry's glyph arena past what the shared arena holds
    s_addText(registry, 3 * GlyphBuffer::GLYPH_FLOOR);
    GlyphBuffer &glyphs = *registry.getGlyphBuffer();
    ASSERT_GT(static_cast<size_t>(glyphs.glyphCapacity()) * sizeof(GlyphQuad),
              backend.byBinding(GLYPH_BINDING)->bytes.size() - GlyphBuffer::GLYPH_FLOOR * sizeof(GlyphQuad));

    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, window.get());

    EXPECT_FALSE(glyphs.glyphDirty().empty());
    EXPECT_FALSE(glyphs.sliceDirty().empty());
    ASSERT_EQ(hub.drawList(window.get()).entries.size(), 1u);
    EXPECT_EQ(hub.drawList(window.get()).entries[0].sliceBase, FrameDrawEntry::NO_TEXT);

    // once the arena can grow the retried upload lands and the text is drawn
    backend.allowGrow = true;
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, window.get());

    EXPECT_TRUE(glyphs.glyphDirty().empty());
    ASSERT_EQ(hub.drawList(window.get()).entries.size(), 1u);
    EXPECT_NE(hub.drawList(window.get()).entries[0].sliceBase, FrameDrawEntry::NO_TEXT);
    EXPECT_EQ(backend.outOfBoundsUploads, 0u);
}

TEST(GpuResourceHub, ReallocatedInstanceBlockIsNotHandedToAnotherRegistryInTheSameFrame)
{
    FakeBackend backend;
    GpuResourceHub hub;
    hub.init(backend);
    auto first = std::make_unique<Window>();
    auto second = std::make_unique<Window>();

    s_submit(*first->geometryRegistry(), 1);
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, first.get());
    ASSERT_EQ(hub.drawList(first.get()).entries.size(), 1u);
    uint32_t oldFirst = hub.drawList(first.get()).entries[0].firstInstance;

    // the first registry outgrows its block, which is freed while the previous frame may
    // still be reading it; a registry allocating in the same frame must not land on it
    hub.syncShared(nullptr);
    s_submit(*first->geometryRegistry(), INITIAL_REGISTRY_INSTANCES);
    hub.syncWindow(nullptr, first.get());
    s_submit(*second->geometryRegistry(), 1);
    hub.syncWindow(nullptr, second.get());

    for (const FrameDrawEntry &entry : hub.drawList(second.get()).entries) {
        bool overlaps = entry.firstInstance < oldFirst + INITIAL_REGISTRY_INSTANCES &&
                        oldFirst < entry.firstInstance + INITIAL_REGISTRY_INSTANCES;
        EXPECT_FALSE(overlaps) << "second registry reuses the block freed this frame at " << oldFirst;
    }
    EXPECT_EQ(backend.outOfBoundsUploads, 0u);
}

TEST(GpuResourceHub, DestroyedRegistryBlocksAreRetiredNotReusedImmediately)
{
    FakeBackend backend;
    GpuResourceHub hub;
    hub.init(backend);
    auto keep = std::make_unique<Window>();
    auto doomed = std::make_unique<Window>();

    s_submit(*doomed->geometryRegistry(), 1);
    hub.syncShared(nullptr);
    hub.syncWindow(nullptr, doomed.get());
    uint32_t doomedFirst = hub.drawList(doomed.get()).entries.at(0).firstInstance;

    hub.syncShared(nullptr);
    doomed.reset();
    s_submit(*keep->geometryRegistry(), 1);
    hub.syncWindow(nullptr, keep.get());
    const FrameDrawEntry *entry = s_entryFor(hub.drawList(keep.get()));
    ASSERT_NE(entry, nullptr);
    EXPECT_NE(entry->firstInstance, doomedFirst);
}

// ---- GradientBuffer ----

TEST(GradientBuffer, SweptSlotIsNotReusedUntilFramesHavePassed)
{
    FakeBackend backend;
    GradientBuffer gradients;
    gradients.init(backend);

    auto a = Gradient::linear(0.0f, {{0.0f, Color4(1.0f, 0.0f, 0.0f)}, {1.0f, Color4(0.0f, 0.0f, 1.0f)}});
    uint32_t slotA = gradients.resolveShared(a);
    ASSERT_NE(slotA, Gradient::INVALID_SLOT);
    a.reset();

    // a frame in flight may still sample a's record; b must not overwrite it this frame
    auto b = Gradient::linear(90.0f, {{0.0f, Color4(0.0f, 1.0f, 0.0f)}, {1.0f, Color4(1.0f, 1.0f, 1.0f)}});
    uint32_t slotB = gradients.resolveShared(b);
    ASSERT_NE(slotB, Gradient::INVALID_SLOT);
    EXPECT_NE(slotB, slotA);

    gradients.advanceFrame();
    gradients.advanceFrame();
    auto c = Gradient::linear(45.0f, {{0.0f, Color4(0.0f, 0.0f, 0.0f)}, {1.0f, Color4(1.0f, 1.0f, 1.0f)}});
    EXPECT_EQ(gradients.resolveShared(c), slotA);
}

// ---- GeometryRegistry ----

TEST(GeometryRegistry, FlushSortsByZIndexAndTracksDirtyIndices)
{
    auto registry = GeometryRegistry::create(nullptr);
    InstanceData high{};
    high.zIndex = 5;
    InstanceData low{};
    low.zIndex = 1;
    GeometryAllocation *h = registry->submit(high);
    registry->submit(low);

    registry->flush();
    ASSERT_EQ(registry->size(), 2u);
    EXPECT_EQ(registry->getAllocations()[0].zIndex, 1);
    EXPECT_EQ(registry->getAllocations()[1].zIndex, 5);
    EXPECT_TRUE(registry->isFullDirty());
    registry->clearDirtyState();

    high.fillColor = 0x12345678u;
    registry->update(*h, high);
    registry->flush();
    EXPECT_FALSE(registry->isFullDirty());
    ASSERT_EQ(registry->getDirtyIndices().size(), 1u);
    EXPECT_EQ(registry->getDirtyIndices()[0], 1u);
    EXPECT_EQ(registry->getAllocations()[1].fillColor, 0x12345678u);
}

TEST(GeometryRegistry, ReleasedSlotIsDroppedAndReused)
{
    auto registry = GeometryRegistry::create(nullptr);
    GeometryAllocation *a = registry->submit(InstanceData{});
    registry->submit(InstanceData{});
    uint32_t slot = a->slotId;
    registry->release(*a);
    registry->flush();
    EXPECT_EQ(registry->size(), 1u);

    GeometryAllocation *c = registry->submit(InstanceData{});
    EXPECT_EQ(c->slotId, slot);
    registry->flush();
    EXPECT_EQ(registry->size(), 2u);
}
