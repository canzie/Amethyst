#include "components/image_button.h"

#include "modules/style.h"
#include "rendering/draw_context.h"

namespace Amethyst {

ImageButton::ImageButton()
{
    resolveStyle();
}

ImageButton::ImageButton(const std::string &svgData)
{
    resolveStyle();
    m_image.setSvg(svgData);
}

void ImageButton::resolveStyle()
{
    resolveBaseStyle(ComponentType::IMAGE_BUTTON);

    auto &style = Style::instance();
    std::span<const StyleKey> classes = getClasses();
    ImageStyleProperties oldBaseline = style.getImageStyle(ComponentType::IMAGE_BUTTON, classes, m_lastResolvedGuiState);
    ImageStyleProperties resolved = style.getImageStyle(ComponentType::IMAGE_BUTTON, classes, effectiveGuiState());
    reconcileStyleOverrides(oldBaseline, resolved, m_image.getImageStyleProperties(),
                           [this](const ImageStyleProperties &next) { setImageStyleProperties(next); });
}

void ImageButton::setSvg(std::string svgData)
{
    if (m_image.setSvg(std::move(svgData))) {
        markDirty();
    }
}

const std::string &ImageButton::getSvg() const
{
    return m_image.getSvg();
}

void ImageButton::setImage(AmTextureId image)
{
    if (m_image.setImage(image)) {
        markDirty();
    }
}

AmTextureId ImageButton::getImage() const
{
    return m_image.getImage();
}

bool ImageButton::setImageStyleProperties(const ImageStyleProperties &props)
{
    bool changed = m_image.setImageStyleProperties(props);
    if (changed) {
        markDirty();
    }
    return changed;
}

const ImageStyleProperties &ImageButton::getImageStyleProperties() const
{
    return m_image.getImageStyleProperties();
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

        m_image.drawImage(ctx, absoluteSize, createInstanceData());
    }

    drawChildren(ctx);

    flags &= ~(FLAG_DIRTY | FLAG_CHILD_DIRTY);
}

} // namespace Amethyst
