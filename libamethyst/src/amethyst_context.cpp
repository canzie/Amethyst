#include "amethyst/amethyst_context.h"

#include "components/ui_base_2d.h"

namespace Amethyst {

AmethystContext::AmethystContext()
{
    m_textProcessor.setGlyphAtlas(&m_glyphAtlas);
    m_drawCtx.textProcessor = &m_textProcessor;
    m_drawCtx.glyphAtlas = &m_glyphAtlas;
    m_drawCtx.svgAtlas = &m_svgAtlas;
}

AmethystContext::~AmethystContext()
{
    FontRegistry::instance().shutdown();
}

FontId AmethystContext::loadFont(std::string_view name, const std::string &path)
{
    return FontRegistry::instance().loadFont(name, path);
}

FontId AmethystContext::loadFont(const std::string &path)
{
    return FontRegistry::instance().loadFont(path);
}

void AmethystContext::init(AmethystBackend &backend)
{
    m_backend = &backend;

    m_svgAtlas.setTextureId(backend.createTexture({m_svgAtlas.getWidth(), m_svgAtlas.getHeight(), AmTextureFormat::RGBA8}));

    m_glyphAtlas.init([&backend] {
        return backend.createTexture({GlyphAtlas::PAGE_SIZE, GlyphAtlas::PAGE_SIZE, AmTextureFormat::R8});
    });

    m_resourceHub.init(backend);
}

void AmethystContext::syncShared(void *cmdBuffer)
{
    if (m_backend == nullptr) {
        return;
    }

    for (uint16_t page = 0; page < m_glyphAtlas.pageCount(); page++) {
        if (m_glyphAtlas.isDirty(page)) {
            m_backend->uploadTexture(cmdBuffer, m_glyphAtlas.getTextureId(page), m_glyphAtlas.getPixels(page));
            m_glyphAtlas.clearDirty(page);
        }
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
