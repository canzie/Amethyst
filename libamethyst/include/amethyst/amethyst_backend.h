/**
 * @file amethyst_backend.h
 * @brief Abstract rendering backend interface for Amethyst
 */

#ifndef AMETHYST__AMETHYST_BACKEND_H
#define AMETHYST__AMETHYST_BACKEND_H

#include "components/common.h"

#include <cstddef>
#include <cstdint>

namespace Amethyst {

/**
 * @brief Generic device primitives every backend implements.
 *
 * All policy (preallocation, suballocation, growth decisions, dirty tracking) lives core-side
 * in GpuArena / GpuResourceHub; backends only create, fill, grow and destroy raw resources.
 */
class AmethystBackend {
  public:
    virtual ~AmethystBackend() = default;

    virtual AmBufferId createBuffer(const AmBufferDesc &desc) = 0;

    /**
     * @brief Recreate a buffer with a larger capacity, preserving its live contents and
     *        rebinding the descriptor recorded in its desc.
     * @return True on success, false if the buffer cannot grow.
     */
    virtual bool growBuffer(AmBufferId id, size_t newCapacity) = 0;

    /**
     * @brief Copy a byte range into a buffer.
     *
     * On a HOST_VISIBLE buffer this is a memcpy into mapped memory plus flush; cmdBuffer is
     * unused. On a DEVICE_LOCAL buffer it is a staged copy that may submit and stall on its
     * own transfer, independent of cmdBuffer.
     */
    virtual void uploadBufferRange(void *cmdBuffer, AmBufferId id, const void *data, size_t offsetBytes, size_t sizeBytes) = 0;

    virtual void destroyBuffer(AmBufferId id) = 0;

    virtual AmTextureId createTexture(const AmTextureDesc &desc) = 0;

    /**
     * @brief Upload the full pixel contents of a texture. cmdBuffer is the backend-native
     *        command buffer the copy is recorded into (e.g. VkCommandBuffer).
     */
    virtual void uploadTexture(void *cmdBuffer, AmTextureId id, const uint8_t *pixels) = 0;

    virtual void destroyTexture(AmTextureId id) = 0;
};

} // namespace Amethyst

#endif // AMETHYST__AMETHYST_BACKEND_H
