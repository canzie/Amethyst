#include "rendering/gpu_resource_hub.h"

#include "amethyst/amethyst_backend.h"
#include "components/ui_layer.h"
#include "logging/log.h"
#include "modules/glyph_buffer.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"
#include "utils/profiling.h"

#include <algorithm>
#include <cstdint>

namespace Amethyst {

GpuResourceHub *GpuResourceHub::s_active = nullptr;

constexpr size_t INSTANCE_ARENA_BYTES = 8 * 1024 * 1024;
constexpr size_t INSTANCE_ARENA_MAX_BYTES = 64 * 1024 * 1024;
constexpr size_t INITIAL_REGISTRY_INSTANCES = 256;
constexpr uint32_t QUAD_INDEX_COUNT = 6;

GpuResourceHub::~GpuResourceHub()
{
    if (s_active == this) {
        s_active = nullptr;
    }
}

void GpuResourceHub::init(AmethystBackend &backend)
{
    m_backend = &backend;
    s_active = this;

    const size_t glyphBytes = GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad);
    const size_t lineBytes = GlyphBuffer::LINE_CAPACITY * sizeof(GlyphLine);
    const size_t sliceBytes = GlyphBuffer::SLICE_CAPACITY * sizeof(GlyphSlice);

    m_instances.init(backend, {INSTANCE_ARENA_BYTES, AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 0},
                     GrowthPolicy::doubleUntil(INSTANCE_ARENA_MAX_BYTES));
    m_glyphs.init(backend, {4 * glyphBytes, AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 2},
                  GrowthPolicy::doubleUntil(32 * glyphBytes));
    m_lines.init(backend, {8 * lineBytes, AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 3},
                 GrowthPolicy::doubleUntil(64 * lineBytes));
    m_slices.init(backend, {8 * sliceBytes, AmBufferUsage::STORAGE, AmBufferMemory::HOST_VISIBLE, 4},
                  GrowthPolicy::doubleUntil(64 * sliceBytes));
    m_gradients.init(backend);

    constexpr uint32_t quadIndices[QUAD_INDEX_COUNT] = {0, 1, 2, 0, 2, 3};
    m_quadIndexBuffer = backend.createBuffer({sizeof(quadIndices), AmBufferUsage::INDEX, AmBufferMemory::DEVICE_LOCAL, UINT32_MAX});
    backend.uploadBufferRange(nullptr, m_quadIndexBuffer, quadIndices, 0, sizeof(quadIndices));

    m_drawList.indexBuffer = m_quadIndexBuffer;
}

void GpuResourceHub::sync(void *cmdBuffer)
{
    AM_PROFILE_FUNCTION();
    m_drawList.entries.clear();

    if (m_backend == nullptr) {
        return;
    }

    m_gradients.sync(cmdBuffer);

    for (GeometryRegistry *registry : GeometryRegistry::getRegistries()) {
        UILayer *layer = registry->getOwningLayer();
        if (layer == nullptr || !layer->isVisible()) {
            continue;
        }

        GeometryBlocks *geometry = obtainGeometryBlocks(registry);
        if (geometry != nullptr) {
            syncGeometry(cmdBuffer, *registry, *geometry);
        }

        GlyphBuffer *glyphBuffer = registry->getGlyphBuffer();
        TextBlocks *text = nullptr;
        if (glyphBuffer != nullptr) {
            text = obtainTextBlocks(registry);
            if (text != nullptr) {
                syncText(cmdBuffer, *glyphBuffer, *text);
            }
        }

        if (geometry != nullptr && geometry->instanceCount > 0) {
            FrameDrawEntry entry;
            entry.firstInstance = static_cast<uint32_t>(geometry->instances.offset / sizeof(InstanceData));
            entry.instanceCount = static_cast<uint32_t>(geometry->instanceCount);
            if (text != nullptr) {
                entry.glyphBase = static_cast<uint32_t>(text->glyph.offset / sizeof(GlyphQuad));
                entry.lineBase = static_cast<uint32_t>(text->line.offset / sizeof(GlyphLine));
                entry.sliceBase = static_cast<uint32_t>(text->slice.offset / sizeof(GlyphSlice));
            }
            m_drawList.entries.push_back(entry);
        }
    }
}

void GpuResourceHub::onRegistryDestroyed(GeometryRegistry *registry)
{
    auto geometryIt = m_geometryBlocks.find(registry);
    if (geometryIt != m_geometryBlocks.end()) {
        m_instances.free(geometryIt->second.instances);
        m_geometryBlocks.erase(geometryIt);
    }

    auto textIt = m_textBlocks.find(registry);
    if (textIt != m_textBlocks.end()) {
        m_glyphs.free(textIt->second.glyph);
        m_lines.free(textIt->second.line);
        m_slices.free(textIt->second.slice);
        m_textBlocks.erase(textIt);
    }
}

GpuResourceHub::GeometryBlocks *GpuResourceHub::obtainGeometryBlocks(GeometryRegistry *registry)
{
    auto it = m_geometryBlocks.find(registry);
    if (it != m_geometryBlocks.end()) {
        return &it->second;
    }

    GeometryBlocks blocks;
    blocks.instances = m_instances.alloc(INITIAL_REGISTRY_INSTANCES * sizeof(InstanceData));
    if (!blocks.instances.isValid()) {
        return nullptr;
    }

    auto [inserted, _] = m_geometryBlocks.emplace(registry, blocks);
    return &inserted->second;
}

GpuResourceHub::TextBlocks *GpuResourceHub::obtainTextBlocks(GeometryRegistry *registry)
{
    auto it = m_textBlocks.find(registry);
    if (it != m_textBlocks.end()) {
        return &it->second;
    }

    TextBlocks blocks;
    blocks.glyph = m_glyphs.alloc(GlyphBuffer::GLYPH_CAPACITY * sizeof(GlyphQuad));
    blocks.line = m_lines.alloc(GlyphBuffer::LINE_CAPACITY * sizeof(GlyphLine));
    blocks.slice = m_slices.alloc(GlyphBuffer::SLICE_CAPACITY * sizeof(GlyphSlice));

    if (!blocks.glyph.isValid() || !blocks.line.isValid() || !blocks.slice.isValid()) {
        m_glyphs.free(blocks.glyph);
        m_lines.free(blocks.line);
        m_slices.free(blocks.slice);
        AM_LOG_ERROR("Failed to allocate text GPU buffers for registry");
        return nullptr;
    }

    auto [inserted, _] = m_textBlocks.emplace(registry, blocks);
    return &inserted->second;
}

void GpuResourceHub::syncGeometry(void *cmdBuffer, GeometryRegistry &registry, GeometryBlocks &blocks)
{
    AM_PROFILE_FUNCTION();
    registry.flush();
    const auto &allocations = registry.getAllocations();
    size_t requiredSize = allocations.size() * sizeof(InstanceData);
    bool fullDirty = registry.isFullDirty();
    const auto &dirtyIndices = registry.getDirtyIndices();

    if (requiredSize > blocks.instances.capacity) {
        m_instances.free(blocks.instances);
        AM_LOG_TRACE("Instance block needs to be resized");

        blocks.instances = m_instances.alloc(requiredSize * 2);
        if (!blocks.instances.isValid()) {
            AM_LOG_ERROR("Failed to reallocate geometry buffer");
            registry.clearDirtyState();
            blocks.instanceCount = 0;
            return;
        }
        fullDirty = true;
    }

    if (!allocations.empty()) {
        if (fullDirty) {
            m_instances.upload(cmdBuffer, allocations.data(), blocks.instances.offset, requiredSize);
        } else if (!dirtyIndices.empty()) {
            auto [loIt, hiIt] = std::minmax_element(dirtyIndices.begin(), dirtyIndices.end());
            size_t lo = *loIt;
            size_t count = static_cast<size_t>(*hiIt) - lo + 1;
            m_instances.upload(cmdBuffer, allocations.data() + lo, blocks.instances.offset + lo * sizeof(InstanceData),
                               count * sizeof(InstanceData));
        }
    }

    registry.clearDirtyState();
    blocks.instanceCount = allocations.size();
}

void GpuResourceHub::syncText(void *cmdBuffer, GlyphBuffer &glyphBuffer, TextBlocks &blocks)
{
    struct UploadSlot {
        GpuArena *arena;
        const ArenaBlock *block;
        const uint8_t *cpuData;
        size_t elemSize;
        DirtyRange dirty;
    };

    UploadSlot slots[3] = {
        {&m_glyphs, &blocks.glyph, reinterpret_cast<const uint8_t *>(glyphBuffer.glyphData()), sizeof(GlyphQuad),
         glyphBuffer.glyphDirty()},
        {&m_lines, &blocks.line, reinterpret_cast<const uint8_t *>(glyphBuffer.lineData()), sizeof(GlyphLine),
         glyphBuffer.lineDirty()},
        {&m_slices, &blocks.slice, reinterpret_cast<const uint8_t *>(glyphBuffer.sliceData()), sizeof(GlyphSlice),
         glyphBuffer.sliceDirty()},
    };

    for (UploadSlot &slot : slots) {
        if (slot.dirty.empty()) {
            continue;
        }

        size_t loBytes = static_cast<size_t>(slot.dirty.lo) * slot.elemSize;
        size_t hiBytes = static_cast<size_t>(slot.dirty.hi) * slot.elemSize;
        slot.arena->upload(cmdBuffer, slot.cpuData + loBytes, slot.block->offset + loBytes, hiBytes - loBytes);
    }

    glyphBuffer.clearDirty();
}

} // namespace Amethyst
