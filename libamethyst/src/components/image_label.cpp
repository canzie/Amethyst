#include "components/image_label.h"

#include "components/ui_image.h"
#include "modules/style.h"
#include "rendering/draw_context.h"

namespace Amethyst {

ImageLabel::ImageLabel()
{
    propagate(INTERACTION_CATEGORY_MOVE);
    resolveStyle();
    m_image = add<UIImage>();
    m_image->setBaseProperties({.interactable = false, .position = UDim2::fromScale(0.0f), .size = UDim2::fromScale(1.0f, 1.0f)});
    m_image->setBaseStyleProperties({.backgroundTransparency = 0.0f});
}

ImageLabel::ImageLabel(const std::string &svgData)
{
    propagate(INTERACTION_CATEGORY_MOVE);
    resolveStyle();
    m_image = add<UIImage>();
    m_image->setBaseProperties({.interactable = false, .position = UDim2::fromScale(0.0f), .size = UDim2::fromScale(1.0f, 1.0f)});
    m_image->setBaseStyleProperties({.backgroundTransparency = 0.0f});
    m_image->setSvg(svgData);
}

void ImageLabel::resolveStyle()
{
    setBaseStyleProperties(Style::instance().getBaseStyle(ComponentType::IMAGE_LABEL, getClasses()));
}

void ImageLabel::setSvg(std::string svgData)
{
    m_image->setSvg(std::move(svgData));
}

const std::string &ImageLabel::getSvg() const
{
    return m_image->getSvg();
}

void ImageLabel::setImage(AmTextureId image)
{
    m_image->setImage(image);
}

AmTextureId ImageLabel::getImage() const
{
    return m_image->getImage();
}

bool ImageLabel::setImageStyleProperties(const ImageStyleProperties &props)
{
    return m_image->setImageStyleProperties(props);
}

const ImageStyleProperties &ImageLabel::getImageStyleProperties() const
{
    return m_image->getImageStyleProperties();
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
    }

    vec4 childClip = computeChildClipRect();

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
