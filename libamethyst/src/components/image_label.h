/*
 * Image display label
 */

#ifndef AMETHYST__IMAGE_LABEL_H
#define AMETHYST__IMAGE_LABEL_H

#include "components/properties.h"
#include "components/ui_image.h"
#include "components/ui_label.h"

#include <string>

namespace Amethyst {

class ImageLabel : public UILabel {
public:
  ImageLabel();
  explicit ImageLabel(const std::string &svgData);
  ~ImageLabel() override = default;

  void draw(DrawContext &ctx) override;
  void resolveStyle() override;

  void setSvg(std::string svgData);
  const std::string &getSvg() const;

  void setImage(AmTextureId image);
  AmTextureId getImage() const;

  bool setImageStyleProperties(const ImageStylePropertiesArgs &props);
  const ImageStyleProperties &getImageStyleProperties() const;

private:
  UIImage m_image;
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_LABEL_H
