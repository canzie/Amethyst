/*
 * Image rendering primitive shared by image components
 */

#ifndef AMETHYST__UI_IMAGE_H
#define AMETHYST__UI_IMAGE_H

#include "components/properties.h"
#include "components/ui_object.h"

#include <string>

namespace Amethyst {

class UIImage : public UIObject {
  public:
    UIImage();
    explicit UIImage(const std::string &svgData);
    virtual ~UIImage() = default;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;

    void setSvg(std::string svgData);
    const std::string &getSvg() const { return m_svgData; }

    void setImage(AmTextureId image);
    AmTextureId getImage() const { return m_image; }

    bool setImageStyleProperties(const ImageStyleProperties &props);
    const ImageStyleProperties &getImageStyleProperties() const { return m_imgStyle; }

  protected:
    ImageStyleProperties m_imgStyle;
    AmTextureId m_image;

  private:
    void resolveSvg(DrawContext &ctx);

    std::string m_svgData;
    bool m_svgResolved = false;
    vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__UI_IMAGE_H
