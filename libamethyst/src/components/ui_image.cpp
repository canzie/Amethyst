#include "components/ui_image.h"

#include "modules/svg_atlas.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"

namespace Amethyst {

static GeometryAllocation *s_pushData(GeometryRegistry *registry, GeometryAllocation *&alloc, const InstanceData &data)
{
    if (alloc == nullptr) {
        alloc = registry->submit(data);
    } else if (alloc->registry != registry) {
        if (alloc->isValid() && alloc->owning) {
            alloc->registry->release(*alloc);
        }
        alloc = registry->submit(data);
    } else {
        registry->update(*alloc, data);
    }
    return alloc;
}

UIImage::~UIImage()
{
    if (m_alloc != nullptr && m_alloc->isValid() && m_alloc->owning) {
        m_alloc->registry->release(*m_alloc);
    }
}

bool UIImage::setImageStyleProperties(const ImageStylePropertiesArgs &props)
{
    return m_imgStyle.apply(props);
}

bool UIImage::setSvg(std::string svgData)
{
    if (m_svgData == svgData) {
        return false;
    }
    m_svgData = std::move(svgData);
    m_svgResolved = false;
    return true;
}

bool UIImage::setImage(AmTextureId image)
{
    if (m_image.id == image.id) {
        return false;
    }
    m_image = image;
    m_svgData.clear();
    m_svgResolved = false;
    return true;
}

void UIImage::resolveSvg(DrawContext &ctx, vec2 absoluteSize)
{
    if (m_svgResolved || m_svgData.empty() || ctx.svgAtlas == nullptr) {
        return;
    }

    uint32_t w = static_cast<uint32_t>(absoluteSize.x);
    uint32_t h = static_cast<uint32_t>(absoluteSize.y);
    if (w == 0 || h == 0) {
        return;
    }

    const SvgEntry *entry = ctx.svgAtlas->loadFromData(m_svgData, w, h);
    if (entry != nullptr) {
        float aw = static_cast<float>(ctx.svgAtlas->getWidth());
        float ah = static_cast<float>(ctx.svgAtlas->getHeight());
        m_svgUvRect = {entry->atlasX / aw, entry->atlasY / ah, (entry->atlasX + entry->width) / aw,
                       (entry->atlasY + entry->height) / ah};
        m_image = ctx.svgAtlas->getTextureId();
    }

    m_svgResolved = true;
}

void UIImage::drawImage(DrawContext &ctx, vec2 absoluteSize, InstanceData base)
{
    resolveSvg(ctx, absoluteSize);

    base.textureId = m_image.id;

    if (m_svgResolved && !m_svgData.empty()) {
        base.setPrimitiveType(PRIMITIVE_SVG);
        base.setUvRect(m_svgUvRect);
        base.setFillColor(m_imgStyle.imageColor);
    } else {
        base.setPrimitiveType(PRIMITIVE_RECT);
        base.setFillColor(m_imgStyle.imageColor);
    }

    s_pushData(ctx.geometry, m_alloc, base);
}

} // namespace Amethyst
