/*
 * Basic rectangular container
 */

#ifndef AMETHYST__FRAME_H
#define AMETHYST__FRAME_H

#include "components/instance.h"
#include "components/ui_object.h"

namespace Amethyst {

class Frame : public UIObject {
  public:
    Frame() = default;
    Frame(Instance *parent) { setParent(parent); };
    virtual ~Frame() = default;

    void draw(GeometryRegistry &registry) override;
};

} // namespace Amethyst

#endif // AMETHYST__FRAME_H
