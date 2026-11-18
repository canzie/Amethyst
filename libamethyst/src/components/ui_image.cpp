#include "components/ui_image.h"

#include "logging/log.h"
#include "modules/svg_atlas.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"
#include "rendering/instance_data.h"

#include <cmath>

namespace Amethyst {

static constexpr float MIN_SVG_EXTENT = 1.0f;
static constexpr float MAX_SVG_EXTENT = 2048.0f;

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
    m_svgFailedSize = {};
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
    m_svgFailedSize = {};
    return true;
}

void UIImage::resolveSvg(DrawContext &ctx, vec2 absoluteSize)
{
    if (m_svgData.empty() || ctx.svgAtlas == nullptr) {
        return;
    }

    // rasterised at the physical size it covers on screen, not its logical size, so it stays
    // sharp on a scaled display; a new scale asks for a new raster like a resize does
    vec2 physicalSize = absoluteSize * ctx.logicalToPhysicalScale;

    // the cast below is undefined for NaN, infinity and negatives, so the range is checked while these are still floats
    bool finite = std::isfinite(physicalSize.x) && std::isfinite(physicalSize.y);
    if (!finite || physicalSize.x > MAX_SVG_EXTENT || physicalSize.y > MAX_SVG_EXTENT) {
        AM_LOG_WARN("refusing to rasterize an SVG at {}x{}, retrying when it is between {} and {}", physicalSize.x, physicalSize.y,
                    MIN_SVG_EXTENT, MAX_SVG_EXTENT);
        return;
    }

    // a size this small is one that has not been laid out yet, which is not worth a warning every frame
    if (physicalSize.x < MIN_SVG_EXTENT || physicalSize.y < MIN_SVG_EXTENT) {
        return;
    }

    uint32_t w = static_cast<uint32_t>(physicalSize.x);
    uint32_t h = static_cast<uint32_t>(physicalSize.y);

    // rasterized at the size it is drawn at, so a resize asks for a fresh raster; a size that
    // already failed is not retried every frame
    std::array<uint32_t, 2> size = {w, h};
    if ((m_svgResolved && size == m_svgSize) || size == m_svgFailedSize) {
        return;
    }

    const SvgEntry *entry = ctx.svgAtlas->loadFromData(m_svgData, w, h);
    if (entry == nullptr) {
        m_svgResolved = false;
        m_svgFailedSize = size;
        return;
    }
    float aw = static_cast<float>(ctx.svgAtlas->getWidth());
    float ah = static_cast<float>(ctx.svgAtlas->getHeight());
    m_svgUvRect = {entry->atlasX / aw, entry->atlasY / ah, (entry->atlasX + entry->width) / aw,
                   (entry->atlasY + entry->height) / ah};
    m_image = ctx.svgAtlas->getTextureId();
    m_svgSize = size;
    m_svgResolved = true;
}

Color4 UIImage::svgTint(const Color4 &color)
{
    // an SVG instance keeps its UVs in shapeData, the same slot a gradient's index goes in, so a
    // gradient tint is flattened to its first stop instead of overwriting the UVs
    if (!color.hasGradient()) {
        return color;
    }
    Color4 flat(color.r, color.g, color.b, color.a);
    const Gradient &gradient = *color.getGradient();
    if (gradient.stopCount > 0) {
        uint32_t packed = gradient.stops[0].color;
        flat.r *= static_cast<float>(packed & 0xFFu) / 255.0f;
        flat.g *= static_cast<float>((packed >> 8) & 0xFFu) / 255.0f;
        flat.b *= static_cast<float>((packed >> 16) & 0xFFu) / 255.0f;
        flat.a *= static_cast<float>((packed >> 24) & 0xFFu) / 255.0f;
    }
    return flat;
}

void UIImage::drawImage(DrawContext &ctx, vec2 absoluteSize, InstanceData base)
{
    resolveSvg(ctx, absoluteSize);

    // the image is content drawn over the background, so it never repeats with it
    base.clearBackgroundRepeat();
    base.textureId = m_image.id;

    if (!m_svgData.empty()) {
        // until a raster exists (not laid out yet, or rasterizing failed) there is nothing valid
        // to sample, so the instance is kept but hidden rather than drawn with stale UVs
        base.setPrimitiveType(PRIMITIVE_SVG);
        base.setUvRect(m_svgUvRect);
        base.setFillColor(svgTint(m_imgStyle.imageColor));
        if (!m_svgResolved) {
            base.setVisible(false);
        }
    } else {
        base.setPrimitiveType(PRIMITIVE_RECT);
        base.setFillColor(m_imgStyle.imageColor);
    }

    s_pushData(ctx.geometry, m_alloc, base);
}

} // namespace Amethyst
