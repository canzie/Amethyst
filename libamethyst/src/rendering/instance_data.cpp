#include "rendering/instance_data.h"

#include "rendering/gpu_resource_hub.h"

#include <cmath>

namespace Amethyst {

void InstanceData::setFillColor(const Color4 &c)
{
    fillColor = packColor(c);

    if (!c.hasGradient()) {
        return;
    }

    GpuResourceHub *hub = GpuResourceHub::active();
    if (hub == nullptr) {
        return;
    }

    uint32_t slot = hub->gradients().resolveShared(c.getGradient());
    if (slot != Gradient::INVALID_SLOT) {
        setGradientSlot(slot);
    }
}

void InstanceData::setBackgroundRepeat(BackgroundRepeat repeat, vec2 size, vec2 step, vec2 position)
{
    bool repeatX = repeat == BackgroundRepeat::REPEAT || repeat == BackgroundRepeat::REPEAT_X;
    bool repeatY = repeat == BackgroundRepeat::REPEAT || repeat == BackgroundRepeat::REPEAT_Y;
    if (!repeatX && !repeatY) {
        return;
    }
    if (size.x <= 0.0f || size.y <= 0.0f || (repeatX && step.x <= 0.0f) || (repeatY && step.y <= 0.0f)) {
        return;
    }

    // moving a repeating mark by whole steps changes nothing, and a wrapped position stays small
    // enough to keep its precision as a half float
    if (repeatX) {
        position.x -= step.x * std::floor(position.x / step.x);
    }
    if (repeatY) {
        position.y -= step.y * std::floor(position.y / step.y);
    }

    shapeData[0] = packHalf2x16(size);
    shapeData[2] = packHalf2x16(step);
    shapeData[3] = packHalf2x16(position);
    if (repeatX) {
        flags |= INSTANCE_FLAG_REPEAT_X;
    }
    if (repeatY) {
        flags |= INSTANCE_FLAG_REPEAT_Y;
    }
}

} // namespace Amethyst
