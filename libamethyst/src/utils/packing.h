/*
 * GPU data packing utilities for reducing bandwidth
 */

#ifndef AMETHYST__PACKING_H
#define AMETHYST__PACKING_H

#include "components/common.h"

#include "math/math.h"
#include <cstdint>

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
    return packHalf1x16(f);
}

inline uint32_t packU16x2(uint16_t low, uint16_t high)
{
    return static_cast<uint32_t>(low) | (static_cast<uint32_t>(high) << 16);
}

inline uint16_t packRotation(float radians)
{
    float normalized = fract(radians / (2.0f * pi<float>()));
    return static_cast<uint16_t>(normalized * 65535.0f);
}

inline uint32_t packRotationAndBorderThickness(float rotationRadians, float borderThickness)
{
    uint16_t rot = packRotation(rotationRadians);
    uint16_t border = packFloatToHalf(borderThickness);
    return static_cast<uint32_t>(rot) | (static_cast<uint32_t>(border) << 16);
}

inline uint32_t packU8x4(const uvec4 &v)
{
    return (v.x & 0xFFu) | ((v.y & 0xFFu) << 8) | ((v.z & 0xFFu) << 16) | ((v.w & 0xFFu) << 24);
}

} // namespace Amethyst

#endif // AMETHYST__PACKING_H
