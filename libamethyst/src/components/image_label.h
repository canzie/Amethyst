/*
 * Image display label
 */

#ifndef AMETHYST__IMAGE_LABEL_H
#define AMETHYST__IMAGE_LABEL_H

#include "components/instance.h"
#include "components/ui_label.h"

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
    ImageLabel(Instance *parent) { setParent(parent); }
    virtual ~ImageLabel() = default;

    void draw(DrawContext &ctx) override;

  public:
    AmTextureId image;
    Color4 imageColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float imageTransparency = 0.0f;
    ScaleType scaleType = ScaleType::STRETCH;
    glm::vec2 tileSize = {1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_LABEL_H
