/*
 * Shape: a single primitive (circle, triangle, ...) rendered as one instance.
 */

#ifndef AMETHYST__SHAPE_H
#define AMETHYST__SHAPE_H

#include "components/ui_object.h"

namespace Amethyst {

class Shape : public UIObject {
  public:
    explicit Shape(PrimitiveType primitive);

    void draw(DrawContext &ctx) override;

  private:
    PrimitiveType m_primitive;
};

} // namespace Amethyst

#endif // AMETHYST__SHAPE_H
