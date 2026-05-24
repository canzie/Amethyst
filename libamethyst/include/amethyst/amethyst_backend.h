/**
 * @file amethyst_backend.h
 * @brief Abstract rendering backend interface for Amethyst
 */

#ifndef AMETHYST__AMETHYST_BACKEND_H
#define AMETHYST__AMETHYST_BACKEND_H

#include "components/common.h"

#include <cstdint>

namespace Amethyst {

class AmethystBackend {
  public:
    virtual ~AmethystBackend() = default;

    virtual void createAtlasTexture(uint32_t width, uint32_t height) = 0;
    virtual void uploadAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) = 0;
    virtual AmTextureId getAtlasTextureId() const = 0;

    virtual void createSvgAtlasTexture(uint32_t width, uint32_t height) = 0;
    virtual void uploadSvgAtlasData(void *cmdBuffer, const uint8_t *pixels, uint32_t width, uint32_t height) = 0;
    virtual AmTextureId getSvgAtlasTextureId() const = 0;
};

} // namespace Amethyst

#endif // AMETHYST__AMETHYST_BACKEND_H
