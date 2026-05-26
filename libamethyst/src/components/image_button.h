/*
 * Image button
 */

#ifndef AMETHYST__IMAGE_BUTTON_H
#define AMETHYST__IMAGE_BUTTON_H

#include "components/instance.h"
#include "components/properties.h"
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

    bool setImageProperties(const ImageProperties &props);
    const ImageProperties &getImageProperties() const { return m_imgProps; }

  public:
    AmTextureId hoverImage;

  protected:
    ImageProperties m_imgProps;

  private:
    void resolveSvg(DrawContext &ctx);

    std::string m_svgData;
    bool m_svgResolved = false;
    glm::vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_BUTTON_H
