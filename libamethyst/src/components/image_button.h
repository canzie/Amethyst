/*
 * Image button
 */

#ifndef AMETHYST__IMAGE_BUTTON_H
#define AMETHYST__IMAGE_BUTTON_H

#include "components/image_label.h"
#include "components/ui_button.h"

namespace Amethyst {

class ImageButton : public UIButton {
  public:
    ImageButton() = default;
    virtual ~ImageButton() = default;

    void draw(GeometryRegistry& registry) override;

  public:
    std::string image;
    Color4 imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float imageTransparency = 0.0f;
    ScaleType scaleType = ScaleType::STRETCH;
    glm::vec2 tileSize = {1.0f, 1.0f};

    std::string hoverImage;
    std::string pressedImage;
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_BUTTON_H
