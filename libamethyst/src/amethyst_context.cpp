#include "amethyst/amethyst_context.h"

#include "components/ui_base_2d.h"

namespace Amethyst {

AmethystContext::AmethystContext() : m_glyphAtlas(&m_fontLoader)
{
    m_textProcessor.setGlyphAtlas(&m_glyphAtlas);
    m_drawCtx.textProcessor = &m_textProcessor;
    m_drawCtx.glyphAtlas = &m_glyphAtlas;
    m_drawCtx.svgAtlas = &m_svgAtlas;
}

bool AmethystContext::loadFont(const std::string &path)
{
    return m_fontLoader.loadFont(path);
}

void AmethystContext::init(AmethystBackend &backend)
{
    m_backend = &backend;

    m_glyphAtlas.setTextureId(backend.createTexture({m_glyphAtlas.getWidth(), m_glyphAtlas.getHeight(), AmTextureFormat::R8}));
    m_svgAtlas.setTextureId(backend.createTexture({m_svgAtlas.getWidth(), m_svgAtlas.getHeight(), AmTextureFormat::RGBA8}));

    m_resourceHub.init(backend);
}

void AmethystContext::sync(void *cmdBuffer)
{
    if (m_backend == nullptr) {
        return;
    }

    if (m_glyphAtlas.isDirty()) {
        m_backend->uploadTexture(cmdBuffer, m_glyphAtlas.getTextureId(), m_glyphAtlas.getPixels());
        m_glyphAtlas.clearDirty();
    }

    if (m_svgAtlas.isDirty()) {
        m_backend->uploadTexture(cmdBuffer, m_svgAtlas.getTextureId(), m_svgAtlas.getPixels());
        m_svgAtlas.clearDirty();
    }

    m_resourceHub.sync(cmdBuffer);
}

void AmethystContext::draw(UIBase2D &root)
{
    root.draw(m_drawCtx);
}

} // namespace Amethyst
