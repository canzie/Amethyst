/*
 * Image draw helper shared by image components
 */

#ifndef AMETHYST__UI_IMAGE_H
#define AMETHYST__UI_IMAGE_H

#include "components/properties.h"

#include <string>

namespace Amethyst {

struct DrawContext;
struct GeometryAllocation;
struct InstanceData;

class UIImage {
public:
  UIImage() = default;
  ~UIImage();

  bool setSvg(std::string svgData);
  const std::string &getSvg() const { return m_svgData; }

  bool setImage(AmTextureId image);
  AmTextureId getImage() const { return m_image; }

  bool setImageStyleProperties(const ImageStyleProperties &props);
  const ImageStyleProperties &getImageStyleProperties() const {
    return m_imgStyle;
  }

  void drawImage(DrawContext &ctx, vec2 absoluteSize, InstanceData base);

private:
  void resolveSvg(DrawContext &ctx, vec2 absoluteSize);

  ImageStyleProperties m_imgStyle;
  AmTextureId m_image;
  std::string m_svgData;
  bool m_svgResolved = false;
  vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
  GeometryAllocation *m_alloc = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__UI_IMAGE_H
