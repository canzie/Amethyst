/*
 * GPU instance data for UI rendering with packed format
 */

#ifndef AMETHYST__INSTANCE_DATA_H
#define AMETHYST__INSTANCE_DATA_H

#include "components/common.h"
#include "utils/packing.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

namespace Amethyst {

struct InstanceData {
    alignas(8) glm::vec2 translation;     // 8B position
    alignas(8) glm::vec2 scale;           // 8B size
    alignas(16) glm::vec4 clipRect;       // 16B clip rect (keep as float)
    uint32_t fillColor = 0xFFFFFFFF;      // 4B packed RGBA
    uint32_t borderColor = 0;             // 4B packed RGBA
    uint32_t shapeData[4] = {0, 0, 0, 0}; // 16B packed half2 pairs for triangle/line/text UV
    uint32_t rotationBorderThickness = 0; // 4B: rotation(16 bits) | borderThickness(16 bits half)
    uint32_t cornerPrimitiveMode = 0;     // 4B: cornerRadius(16 bits half) | primitiveType(8) | borderMode(8)
    uint32_t textureId = UINT32_MAX;      // 4B texture handle
    int32_t zIndex = 0;                   // 4B z-index for sorting

    void setFillColor(const Color4 &c) { fillColor = packColor(c); }
    void setBorderColor(const Color4 &c) { borderColor = packColor(c); }

    void setRotation(float radians)
    {
        uint16_t rot = packRotation(radians);
        rotationBorderThickness = (rotationBorderThickness & 0xFFFF0000u) | rot;
    }

    void setBorderThickness(float thickness)
    {
        uint16_t packed = packFloatToHalf(thickness);
        rotationBorderThickness = (rotationBorderThickness & 0x0000FFFFu) | (static_cast<uint32_t>(packed) << 16);
    }

    void setCornerRadius(float radius)
    {
        uint16_t packed = packFloatToHalf(radius);
        cornerPrimitiveMode = (cornerPrimitiveMode & 0xFFFF0000u) | packed;
    }

    void setPrimitiveType(PrimitiveType type)
    {
        cornerPrimitiveMode = (cornerPrimitiveMode & 0xFF00FFFFu) | (static_cast<uint32_t>(type) << 16);
    }

    void setBorderMode(BorderMode mode)
    {
        cornerPrimitiveMode = (cornerPrimitiveMode & 0x00FFFFFFu) | (static_cast<uint32_t>(mode) << 24);
    }

    void setUvRect(const glm::vec4 &uvRect)
    {
        shapeData[0] = glm::packHalf2x16(glm::vec2(uvRect.x, uvRect.y));
        shapeData[1] = glm::packHalf2x16(glm::vec2(uvRect.z, uvRect.w));
    }
};

} // namespace Amethyst

#endif // AMETHYST__INSTANCE_DATA_H
