/*
 * ImageLabel implementation
 */

#include "components/image_label.h"

#include "modules/svg_atlas.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

ImageLabel::ImageLabel(const std::string &svgData) : m_svgData(svgData)
{
    m_imgProps.imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    m_imgProps.imageTransparency = 0.0f;
    m_imgProps.scaleType = ImageScaleType::STRETCH;
    m_imgProps.tileSize = {1.0f, 1.0f};
}

bool ImageLabel::setImageProperties(const ImageProperties &props)
{
    bool changed = false;
#define AM_APPLY(field) \
    if (propIsSet(props.field) && m_imgProps.field != props.field) { \
        m_imgProps.field = props.field; \
        changed = true; \
    }
    AM_APPLY(imageColor)
    AM_APPLY(imageTransparency)
    AM_APPLY(scaleType)
    AM_APPLY(tileSize)
#undef AM_APPLY
    if (!props.svg.empty() && m_imgProps.svg != props.svg) {
        m_imgProps.svg = props.svg;
        m_svgData = props.svg;
        m_svgResolved = false;
        changed = true;
    }
    if (props.image.isValid() && m_imgProps.image.id != props.image.id) {
        m_imgProps.image = props.image;
        changed = true;
    }
    if (changed) {
        markDirty();
    }
    return changed;
}

void ImageLabel::setSvg(const std::string &svgData)
{
    m_svgData = svgData;
    m_svgResolved = false;
    m_imgProps.svg = svgData;
    markDirty();
}

void ImageLabel::resolveSvg(DrawContext &ctx)
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
        m_imgProps.image = ctx.svgAtlas->getTextureId();
    }

    m_svgResolved = true;
}

void ImageLabel::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        resolveSvg(ctx);

        InstanceData data = createInstanceData();
        data.textureId = m_imgProps.image.id;

        if (m_svgResolved && !m_svgData.empty()) {
            data.setPrimitiveType(PRIMITIVE_SVG);
            data.setUvRect(m_svgUvRect);
            data.setFillColor(m_imgProps.imageColor);
        } else {
            data.setPrimitiveType(PRIMITIVE_RECT);
        }

        if (m_geometryAlloc == nullptr) {
            m_geometryAlloc = ctx.geometry->submit(data);
        } else {
            ctx.geometry->update(*m_geometryAlloc, data);
        }
    }

    glm::vec4 childClip = computeChildClipRect();

    for (auto &child : m_children) {
        if (auto *drawable = child->as<UIObject>()) {
            drawable->clipRect = childClip;
            drawable->computeAbsolutes(absoluteContentSize, absoluteContentPosition, absoluteRotation);
            drawable->draw(ctx);
        }
    }

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
