/*
 * Shape: a single primitive (circle, triangle, ...) rendered as one instance.
 */

#ifndef AMETHYST__SHAPE_H
#define AMETHYST__SHAPE_H

#include "components/ui_object.h"

namespace Amethyst {

class Shape : public UIObject {
  public:
    explicit Shape(ShapeKind kind);

    void draw(DrawContext &ctx) override;

    bool setKind(ShapeKind kind);
    ShapeKind getKind() const { return m_kind; }

  private:
    ShapeKind m_kind;
    PrimitiveType m_primitive;
};

} // namespace Amethyst

#endif // AMETHYST__SHAPE_H
