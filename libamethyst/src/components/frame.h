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
    Frame(Instance *parent)
    {
        setParent(parent);
        name = "Frame";
    };
    virtual ~Frame() = default;

    void draw(GeometryRegistry &registry) override;

    void onMouseButton1Click() override;
    void onMouseButton2Click() override;
};

} // namespace Amethyst

#endif // AMETHYST__FRAME_H
