#ifndef AMETHYST__CONTAINER_H
#define AMETHYST__CONTAINER_H

#include "components/ui_object.h"

#include <vector>

namespace Amethyst {

class Container : public UIObject {
  public:
    Container();
    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  protected:
    EventResult onMouseScrollUp() override { return EventResult::PROPAGATE; }
    EventResult onMouseScrollDown() override { return EventResult::PROPAGATE; }
};

} // namespace Amethyst

#endif // AMETHYST__CONTAINER_H
