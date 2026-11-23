/*
 * Image draw helper shared by image components
 */

#ifndef AMETHYST__UI_IMAGE_H
#define AMETHYST__UI_IMAGE_H

#include "components/properties.h"

#include <array>
#include <cstdint>
#include <string>

namespace Amethyst {

struct DrawContext;
struct GeometryAllocation;
struct InstanceData;
class UIBase2D;

class UIImage {
public:
  UIImage() = default;
  ~UIImage();

  bool setSvg(std::string svgData);
  const std::string &getSvg() const { return m_svgData; }

  bool setImage(AmTextureId image);
  AmTextureId getImage() const { return m_image; }

  bool setImageStyleProperties(const ImageStylePropertiesArgs &props);
  const ImageStyleProperties &getImageStyleProperties() const {
    return m_imgStyle;
  }

  bool resolveImageStyle(const ImageStyleProperties &resolved) { return m_imgStyle.apply(resolved); }

  /**
   * @brief Draws the image over its owner.
   * @param ctx Draw context
   * @param owner The node the image fills, which also maps it into the window
   * @param base The owner's instance in its layout space, which the image is built on
   */
  void drawImage(DrawContext &ctx, const UIBase2D &owner, InstanceData base);

private:
  /**
   * @brief Rasterises an SVG at the size it covers on screen.
   * @param drawnSize The size it is drawn at in logical window pixels, the owner's scale included
   */
  void resolveSvg(DrawContext &ctx, vec2 drawnSize);

public:
  /**
   * @brief The solid tint an SVG instance is drawn with: the color itself, or for a gradient its
   * first stop times the tint, since SVG instances have no room for a gradient slot.
   */
  static Color4 svgTint(const Color4 &color);

private:

  ImageStyleProperties m_imgStyle;
  AmTextureId m_image;
  std::string m_svgData;
  bool m_svgResolved = false;
  std::array<uint32_t, 2> m_svgSize{};
  std::array<uint32_t, 2> m_svgFailedSize{};
  vec4 m_svgUvRect = {0.0f, 0.0f, 1.0f, 1.0f};
  GeometryAllocation *m_alloc = nullptr;
};

} // namespace Amethyst

#endif // AMETHYST__UI_IMAGE_H
