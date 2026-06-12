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
#include "parsers/freetype/font_loader.h"
#include "rendering/draw_context.h"
#include "rendering/gpu_resource_hub.h"

#include <string>

namespace Amethyst {

class UIBase2D;

class AmethystContext {
  public:
    AmethystContext();
    ~AmethystContext() = default;

    AmethystContext(const AmethystContext &) = delete;
    AmethystContext &operator=(const AmethystContext &) = delete;

    bool loadFont(const std::string &path);
    void init(AmethystBackend &backend);
    void sync(void *cmdBuffer);
    void draw(UIBase2D &root);

    /**
     * @brief The draw list built by the last sync(), consumed by the backend's record pass.
     * @return Per-registry draw entries plus the shared index buffer handle.
     */
    const FrameDrawList &getDrawList() const { return m_resourceHub.drawList(); }

    /**
     * @brief Bindless texture id of the glyph atlas, e.g. for debug visualization.
     * @return Atlas texture id, invalid before init().
     */
    AmTextureId getGlyphAtlasTexture() const { return m_glyphAtlas.getTextureId(); }

  private:
    AmethystBackend *m_backend = nullptr;
    FontLoader m_fontLoader;
    GlyphAtlas m_glyphAtlas;
    TextProcessor m_textProcessor;
    SvgAtlas m_svgAtlas;
    DrawContext m_drawCtx;
    GpuResourceHub m_resourceHub;
};

} // namespace Amethyst

#endif // AMETHYST__AMETHYST_CONTEXT_H
