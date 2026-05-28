#ifndef AMETHYST__CONTAINER_H
#define AMETHYST__CONTAINER_H

#include "components/ui_object.h"

#include <vector>

namespace Amethyst {

class Container : public UIObject {
  public:
    Container() = default;
    void draw(DrawContext &ctx) override;
    std::vector<Instance *> getHittableInstances() override;

  protected:
    EventResult onMouseEnter() override { return EventResult::PROPAGATE; }
    EventResult onMouseLeave() override { return EventResult::PROPAGATE; }
    EventResult onMouseMoved(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }
    EventResult onMouseButton1Down(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }
    EventResult onMouseButton1Up(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }
    EventResult onMouseButton1Click() override { return EventResult::PROPAGATE; }
    EventResult onMouseButton2Down(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }
    EventResult onMouseButton2Up(uint32_t, uint32_t) override { return EventResult::PROPAGATE; }
    EventResult onMouseButton2Click() override { return EventResult::PROPAGATE; }
    EventResult onMouseScrollUp() override { return EventResult::PROPAGATE; }
    EventResult onMouseScrollDown() override { return EventResult::PROPAGATE; }
};

} // namespace Amethyst

#endif // AMETHYST__CONTAINER_H
