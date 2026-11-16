#include "components/image_label.h"

#include "modules/style.h"
#include "rendering/draw_context.h"

namespace Amethyst {

// local draw layer of the image, over the node's own quad (see UIObject::drawZ)
static constexpr uint32_t IMAGE_DRAW_LAYER = 1;

ImageLabel::ImageLabel()
{
    resolveStyle();
}

ImageLabel::ImageLabel(const std::string &svgData)
{
    resolveStyle();
    m_image.setSvg(svgData);
}

void ImageLabel::resolveStyle()
{
    resolveBaseStyle(ComponentType::IMAGE_LABEL);

    ImageStyleProperties resolved =
        Style::instance().getImageStyle(ComponentType::IMAGE_LABEL, getClasses(), effectiveGuiState(), m_part);
    if (m_image.resolveImageStyle(resolved)) {
        markDirty();
    }
}

void ImageLabel::setSvg(std::string svgData)
{
    if (m_image.setSvg(std::move(svgData))) {
        markDirty();
    }
}

const std::string &ImageLabel::getSvg() const
{
    return m_image.getSvg();
}

void ImageLabel::setImage(AmTextureId image)
{
    if (m_image.setImage(image)) {
        markDirty();
    }
}

AmTextureId ImageLabel::getImage() const
{
    return m_image.getImage();
}

bool ImageLabel::setImageStyleProperties(const ImageStylePropertiesArgs &props)
{
    bool changed = m_image.setImageStyleProperties(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

const ImageStyleProperties &ImageLabel::getImageStyleProperties() const
{
    return m_image.getImageStyleProperties();
}

void ImageLabel::draw(DrawContext &ctx)
{
    if (!(flags & (FLAG_DIRTY | FLAG_CHILD_DIRTY))) {
        return;
    }

    if (flags & FLAG_DIRTY) {
        InstanceData data = createInstanceData();
        data.setPrimitiveType(PRIMITIVE_RECT);
        pushData(ctx.geometry, data);

        InstanceData imageData = createInstanceData();
        imageData.zIndex = drawZ(IMAGE_DRAW_LAYER);
        m_image.drawImage(ctx, absoluteSize, imageData);
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
