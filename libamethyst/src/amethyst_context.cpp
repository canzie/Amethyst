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

void AmethystContext::syncShared(void *cmdBuffer)
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

    m_resourceHub.syncShared(cmdBuffer);
}

void AmethystContext::syncWindow(void *cmdBuffer, Window &window)
{
    if (m_backend == nullptr) {
        return;
    }

    m_resourceHub.syncWindow(cmdBuffer, &window);
}

void AmethystContext::draw(UIBase2D &root)
{
    root.arrange();
    root.draw(m_drawCtx);
}

} // namespace Amethyst
