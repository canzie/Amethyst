/*
 * Image display label
 */

#ifndef AMETHYST__IMAGE_LABEL_H
#define AMETHYST__IMAGE_LABEL_H

#include "components/ui_label.h"
#include <string>

namespace Amethyst {

enum class ScaleType : uint8_t {
    STRETCH,
    TILE,
    FIT,
    CROP,
};

class ImageLabel : public UILabel {
  public:
    ImageLabel() = default;
    virtual ~ImageLabel() = default;

    void draw(GeometryRegistry& registry) override;

  public:
    std::string image;
    Color4 imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float imageTransparency = 0.0f;
    ScaleType scaleType = ScaleType::STRETCH;
    glm::vec2 tileSize = {1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_LABEL_H
