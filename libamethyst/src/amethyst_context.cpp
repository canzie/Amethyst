#include "amethyst/amethyst_context.h"

#include "components/ui_base_2d.h"

namespace Amethyst {

AmethystContext::AmethystContext()
    : m_glyphAtlas(&m_fontLoader)
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

    backend.createAtlasTexture(m_glyphAtlas.getWidth(), m_glyphAtlas.getHeight());
    m_glyphAtlas.setTextureId(backend.getAtlasTextureId());

    backend.createSvgAtlasTexture(m_svgAtlas.getWidth(), m_svgAtlas.getHeight());
    m_svgAtlas.setTextureId(backend.getSvgAtlasTextureId());
}

void AmethystContext::sync(void *cmdBuffer)
{
    if (m_backend == nullptr) {
        return;
    }

    if (m_glyphAtlas.isDirty()) {
        m_backend->uploadAtlasData(cmdBuffer, m_glyphAtlas.getPixels(), m_glyphAtlas.getWidth(), m_glyphAtlas.getHeight());
        m_glyphAtlas.clearDirty();
    }

    if (m_svgAtlas.isDirty()) {
        m_backend->uploadSvgAtlasData(cmdBuffer, m_svgAtlas.getPixels(), m_svgAtlas.getWidth(), m_svgAtlas.getHeight());
        m_svgAtlas.clearDirty();
    }
}

void AmethystContext::draw(UIBase2D &root)
{
    root.draw(m_drawCtx);
}

} // namespace Amethyst
