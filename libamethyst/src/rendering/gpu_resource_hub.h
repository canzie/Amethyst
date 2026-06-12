/**
 * @file gpu_resource_hub.h
 * @brief Owns all shared GPU buffer arenas and per-registry suballocations, and builds the per-frame draw list
 */

#ifndef AMETHYST__GPU_RESOURCE_HUB_H
#define AMETHYST__GPU_RESOURCE_HUB_H

#include "rendering/frame_draw_list.h"
#include "rendering/gpu_arena.h"

#include <unordered_map>

namespace Amethyst {

class AmethystBackend;
class GeometryRegistry;
class GlyphBuffer;

/**
 * @brief Core-side policy layer over the backend's generic buffer primitives.
 *
 * Holds the instance/glyph/line/slice arenas and the quad index buffer, maps each
 * GeometryRegistry to its suballocated blocks, flushes dirty producer data through
 * uploadBufferRange in sync(), and emits a FrameDrawList for the backend's record pass.
 */
class GpuResourceHub {
  public:
    ~GpuResourceHub();

    /**
     * @brief Create all shared buffers on the backend and register this hub as the active one.
     * @param backend Backend the buffers live on.
     */
    void init(AmethystBackend &backend);

    /**
     * @brief Flush all dirty registry and text data to the GPU and rebuild the draw list.
     * @param cmdBuffer Backend-native command buffer, forwarded to upload calls.
     */
    void sync(void *cmdBuffer);

    /**
     * @brief The draw list built by the last sync().
     * @return Per-registry draw entries plus the shared index buffer handle.
     */
    const FrameDrawList &drawList() const { return m_drawList; }

    /**
     * @brief The hub registered by the most recent init(), used by registry destructors.
     * @return Active hub, or nullptr if no context has initialized one.
     */
    static GpuResourceHub *active() { return s_active; }

    /**
     * @brief Release all blocks suballocated for a registry. Called from ~GeometryRegistry.
     * @param registry Registry being destroyed.
     */
    void onRegistryDestroyed(GeometryRegistry *registry);

  private:
    struct GeometryBlocks {
        ArenaBlock instances;
        size_t instanceCount = 0;
    };

    struct TextBlocks {
        ArenaBlock glyph;
        ArenaBlock line;
        ArenaBlock slice;
    };

    GeometryBlocks *obtainGeometryBlocks(GeometryRegistry *registry);
    TextBlocks *obtainTextBlocks(GeometryRegistry *registry);
    void syncGeometry(void *cmdBuffer, GeometryRegistry &registry, GeometryBlocks &blocks);
    void syncText(void *cmdBuffer, GlyphBuffer &glyphBuffer, TextBlocks &blocks);

    static GpuResourceHub *s_active;

    AmethystBackend *m_backend = nullptr;

    GpuArena m_instances;
    GpuArena m_glyphs;
    GpuArena m_lines;
    GpuArena m_slices;
    AmBufferId m_quadIndexBuffer;

    std::unordered_map<GeometryRegistry *, GeometryBlocks> m_geometryBlocks;
    std::unordered_map<GeometryRegistry *, TextBlocks> m_textBlocks;

    FrameDrawList m_drawList;
};

} // namespace Amethyst

#endif // AMETHYST__GPU_RESOURCE_HUB_H
