/*
 * GPU data packing utilities for reducing bandwidth
 */

#ifndef AMETHYST__PACKING_H
#define AMETHYST__PACKING_H

#include "components/common.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

namespace Amethyst {

inline uint32_t packColor(const Color4 &c)
{
    uint32_t r = static_cast<uint32_t>(c.r * 255.0f) & 0xFF;
    uint32_t g = static_cast<uint32_t>(c.g * 255.0f) & 0xFF;
    uint32_t b = static_cast<uint32_t>(c.b * 255.0f) & 0xFF;
    uint32_t a = static_cast<uint32_t>(c.a * 255.0f) & 0xFF;
    return r | (g << 8) | (b << 16) | (a << 24);
}

inline uint32_t packColor(const Color3 &c)
{
    return packColor(Color4(c, 1.0f));
}

inline uint32_t packColor(int r, int g, int b, int a)
{
    return packColor(Color4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f));
}

inline uint16_t packFloatToHalf(float f)
{
    return glm::packHalf1x16(f);
}

inline uint16_t packRotation(float radians)
{
    float normalized = glm::fract(radians / (2.0f * glm::pi<float>()));
    return static_cast<uint16_t>(normalized * 65535.0f);
}

inline uint32_t packRotationAndBorderThickness(float rotationRadians, float borderThickness)
{
    uint16_t rot = packRotation(rotationRadians);
    uint16_t border = packFloatToHalf(borderThickness);
    return static_cast<uint32_t>(rot) | (static_cast<uint32_t>(border) << 16);
}

inline uint32_t packCornerPrimitiveMode(float cornerRadius, uint8_t primitiveType, uint8_t borderMode)
{
    uint16_t corner = packFloatToHalf(cornerRadius);
    return static_cast<uint32_t>(corner) | (static_cast<uint32_t>(primitiveType) << 16) | (static_cast<uint32_t>(borderMode) << 24);
}

} // namespace Amethyst

#endif // AMETHYST__PACKING_H
