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
    EventResult onMouseMoved(int32_t, int32_t) override { return EventResult::PROPAGATE; }
    EventResult onInputBegan(const InputObject &) override { return EventResult::PROPAGATE; }
    EventResult onInputChanged(const InputObject &) override { return EventResult::PROPAGATE; }
    EventResult onInputEnded(const InputObject &) override { return EventResult::PROPAGATE; }
    EventResult onMouseScrollUp() override { return EventResult::PROPAGATE; }
    EventResult onMouseScrollDown() override { return EventResult::PROPAGATE; }
};

} // namespace Amethyst

#endif // AMETHYST__CONTAINER_H
