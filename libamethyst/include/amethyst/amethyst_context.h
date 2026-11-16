/**
 * @file amethyst_context.h
 * @brief Core context that owns all internal Amethyst state
 */

#ifndef AMETHYST__AMETHYST_CONTEXT_H
#define AMETHYST__AMETHYST_CONTEXT_H

#include "amethyst_backend.h"
#include "modules/glyph_atlas.h"
#include "modules/svg_atlas.h"
#include "modules/text_processor.h"
#include "parsers/freetype/font_registry.h"
#include "rendering/draw_context.h"
#include "rendering/gpu_resource_hub.h"

#include <string>
#include <string_view>

namespace Amethyst {

class UIBase2D;
class Window;

class AmethystContext {
  public:
    AmethystContext();
    ~AmethystContext();

    AmethystContext(const AmethystContext &) = delete;
    AmethystContext &operator=(const AmethystContext &) = delete;

    /**
     * @brief Load a font under the family and style its file declares, e.g. "IBM Plex Mono Bold".
     * @param path Path to the font file
     * @return Id of the font, or an invalid id if the file could not be loaded
     */
    FontId loadFont(const std::string &path);

    /**
     * @brief Load a font under a chosen name, for themes to select with font-family.
     * @param name Name to register the font under, in place of the one its file declares
     * @param path Path to the font file
     * @return Id of the font, or an invalid id if the file could not be loaded
     */
    FontId loadFont(std::string_view name, const std::string &path);

    void init(AmethystBackend &backend);

    /**
     * @brief Flush the glyph/SVG atlas textures and the shared gradient buffer.
     *
     * Call exactly once per frame, before syncWindow() for any of that frame's windows, after
     * waiting on the fence of the frame that last used cmdBuffer. It records texture copies and
     * barriers, so cmdBuffer must not be inside a render pass (Vulkan: call it before
     * vkCmdBeginRendering). It also starts a new frame for the GPU buffer arenas, see
     * setFramesInFlight().
     * @param cmdBuffer Backend-native command buffer, forwarded to upload calls.
     */
    void syncShared(void *cmdBuffer);

    /**
     * @brief Flush dirty registry/text data for one window and rebuild its draw list.
     *
     * Like syncShared(), call it outside a render pass: a backend may record uploads into cmdBuffer.
     * @param cmdBuffer Backend-native command buffer, forwarded to upload calls.
     * @param window Window whose registries to flush.
     */
    void syncWindow(void *cmdBuffer, Window &window);

    /**
     * @brief How many frames the GPU may still be reading while the next one is prepared.
     *
     * Freed GPU buffer blocks are only reused after this many syncShared() calls. Match the
     * swapchain loop's frames in flight (default 2).
     * @param frames Frames in flight.
     */
    void setFramesInFlight(uint32_t frames) { m_resourceHub.setFramesInFlight(frames); }

    void draw(UIBase2D &root);

    /**
     * @brief The draw list built by the last syncWindow() for a given window.
     * @param window Window whose draw list to return.
     * @return Per-registry draw entries plus the shared index buffer handle.
     */
    const FrameDrawList &getDrawList(Window &window) const { return m_resourceHub.drawList(&window); }

    /**
     * @brief Bindless texture id of a glyph atlas page, e.g. for debug visualization.
     * @param page Page to look up
     * @return Atlas texture id, invalid before init().
     */
    AmTextureId getGlyphAtlasTexture(uint16_t page = 0) const { return m_glyphAtlas.getTextureId(page); }

    /**
     * @brief The shaper text is laid out and measured through.
     */
    const TextProcessor &getTextProcessor() const { return m_textProcessor; }

  private:
    AmethystBackend *m_backend = nullptr;
    GlyphAtlas m_glyphAtlas;
    TextProcessor m_textProcessor;
    SvgAtlas m_svgAtlas;
    DrawContext m_drawCtx;
    GpuResourceHub m_resourceHub;
};

} // namespace Amethyst

#endif // AMETHYST__AMETHYST_CONTEXT_H
