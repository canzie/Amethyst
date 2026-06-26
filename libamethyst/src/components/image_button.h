/*
 * Image button
 */

#ifndef AMETHYST__IMAGE_BUTTON_H
#define AMETHYST__IMAGE_BUTTON_H

#include "components/properties.h"
#include "components/ui_button.h"
#include "components/ui_image.h"

#include <string>

namespace Amethyst {

class ImageButton : public UIButton {
public:
  ImageButton();
  explicit ImageButton(const std::string &svgData);
  ~ImageButton() override = default;

  void draw(DrawContext &ctx) override;
  void resolveStyle() override;

  void setSvg(std::string svgData);
  const std::string &getSvg() const;

  void setImage(AmTextureId image);
  AmTextureId getImage() const;

  bool setImageStyleProperties(const ImageStyleProperties &props);
  const ImageStyleProperties &getImageStyleProperties() const;

private:
  UIImage m_image;
};

} // namespace Amethyst

#endif // AMETHYST__IMAGE_BUTTON_H
