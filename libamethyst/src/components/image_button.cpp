#include "components/image_button.h"

#include "components/ui_image.h"
#include "modules/style.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

namespace Amethyst {

ImageButton::ImageButton()
{
    resolveStyle();
    m_image = add<UIImage>();
    m_image->setBaseProperties({.interactable = false, .position = UDim2::fromScale(0.0f),
                                .size = UDim2::fromScale(1.0f, 1.0f)});
    m_image->setBaseStyleProperties({.backgroundTransparency = 1.0f});
}

ImageButton::ImageButton(const std::string &svgData)
{
    resolveStyle();
    m_image = add<UIImage>();
    m_image->setBaseProperties({.interactable = false, .position = UDim2::fromScale(0.0f),
                                .size = UDim2::fromScale(1.0f, 1.0f)});
    m_image->setBaseStyleProperties({.backgroundTransparency = 1.0f});
    m_image->setSvg(svgData);
}

void ImageButton::resolveStyle()
{
    setBaseStyleProperties(Style::instance().getBaseStyle(ComponentType::IMAGE_BUTTON, getClasses()));
}

void ImageButton::setSvg(std::string svgData)
{
    m_image->setSvg(std::move(svgData));
}

const std::string &ImageButton::getSvg() const
{
    return m_image->getSvg();
}

void ImageButton::setImage(AmTextureId image)
{
    m_image->setImage(image);
}

AmTextureId ImageButton::getImage() const
{
    return m_image->getImage();
}

bool ImageButton::setImageStyleProperties(const ImageStyleProperties &props)
{
    return m_image->setImageStyleProperties(props);
}

const ImageStyleProperties &ImageButton::getImageStyleProperties() const
{
    return m_image->getImageStyleProperties();
}

void ImageButton::draw(DrawContext &ctx)
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
