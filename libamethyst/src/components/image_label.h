/*
 * Image display label
 */

#ifndef AMETHYST__IMAGE_LABEL_H
#define AMETHYST__IMAGE_LABEL_H

#include "components/instance.h"
#include "components/properties.h"
#include "components/ui_label.h"

#include <string>

namespace Amethyst {

class ImageLabel : public UILabel {
  public:
    ImageLabel();
    explicit ImageLabel(const std::string &svgData);
    virtual ~ImageLabel() = default;

    void draw(DrawContext &ctx) override;
    void resolveStyle() override;
    EventResult onMouseMoved(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }

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

#endif // AMETHYST__IMAGE_LABEL_H
