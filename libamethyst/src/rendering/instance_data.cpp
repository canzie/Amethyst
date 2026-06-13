#include "rendering/instance_data.h"

#include "rendering/gpu_resource_hub.h"

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

} // namespace Amethyst
