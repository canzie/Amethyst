/*
 * Image button
 */

#ifndef AMETHYST__IMAGE_BUTTON_H
#define AMETHYST__IMAGE_BUTTON_H

#include "components/instance.h"
#include "components/ui_button.h"

#include <string>

namespace Amethyst {

class ImageButton : public UIButton {
  public:
    ImageButton() = default;
    explicit ImageButton(const std::string &svgData);
    virtual ~ImageButton() = default;

    void draw(DrawContext &ctx) override;

    void setSvg(const std::string &svgData);

  public:
    AmTextureId image;
    Color4 imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float imageTransparency = 0.0f;
    ImageScaleType scaleType = ImageScaleType::STRETCH;
    glm::vec2 tileSize = {1.0f, 1.0f};

    AmTextureId hoverImage;

  private:
    void resolveSvg(DrawContext &ctx);

    std::string m_svgData;
    bool m_svgResolved = false;
    glm::vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_BUTTON_H
