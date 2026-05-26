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
    ImageLabel() = default;
    explicit ImageLabel(const std::string &svgData);
    virtual ~ImageLabel() = default;

    void draw(DrawContext &ctx) override;
    EventResult onMouseMoved(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }

    void setSvg(const std::string &svgData);

    bool setImageProperties(const ImageProperties &props);
    const ImageProperties &getImageProperties() const { return m_imgProps; }

  protected:
    ImageProperties m_imgProps;

  private:
    void resolveSvg(DrawContext &ctx);

    std::string m_svgData;
    bool m_svgResolved = false;
    glm::vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_LABEL_H
