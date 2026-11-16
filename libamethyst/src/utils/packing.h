/*
 * GPU data packing utilities for reducing bandwidth
 */

#ifndef AMETHYST__PACKING_H
#define AMETHYST__PACKING_H

#include "components/common.h"

#include "math/math.h"
#include <cstdint>

namespace Amethyst {

/**
 * @brief Quantise a [0, 1] channel to 8 bits, rounding to nearest.
 *
 * Out of range values clamp (colour arithmetic easily overshoots 1.0, which would otherwise
 * wrap to 0, and casting a negative float to unsigned is undefined); NaN maps to 0.
 */
inline uint32_t packUnorm8(float v)
{
    if (!(v > 0.0f)) {
        return 0;
    }
    if (v >= 1.0f) {
        return 255;
    }
    return static_cast<uint32_t>(v * 255.0f + 0.5f);
}

inline uint32_t packColor(const Color4 &c)
{
    return packUnorm8(c.r) | (packUnorm8(c.g) << 8) | (packUnorm8(c.b) << 16) | (packUnorm8(c.a) << 24);
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

inline uint16_t clampToU16(float v)
{
    if (v <= 0.0f) {
        return 0;
    }
    if (v >= 65535.0f) {
        return 65535;
    }
    return static_cast<uint16_t>(v + 0.5f);
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
